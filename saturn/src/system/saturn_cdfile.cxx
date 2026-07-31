#include <srl.hpp>
#include "disc.h"
#include "saturn_audio.h"
#include "cd_music.h"
#include "saturn_platform.h"

extern "C" {
#include <string.h>
}

#define SECTOR_BYTES    2048
#define BOUNCE_SECTORS  16
#define BOUNCE_BYTES    (SECTOR_BYTES * BOUNCE_SECTORS)

#define CACHE_WINDOW_BYTES (SECTOR_BYTES * 32)

struct DiscFile
{
    SRL::Cd::File *File;
    int32_t        Size;
    uint8_t       *Data;
};

#define FILE_CACHE_MAX  (256 * 1024)

static uint8_t *g_bounce = nullptr;

static uint8_t *bounce_buffer()
{
    if (g_bounce == nullptr)
    {
        g_bounce = (uint8_t *)SRL::Memory::LowWorkRam::Malloc(BOUNCE_BYTES);
    }

    return g_bounce;
}

static uint8_t *g_storage = nullptr;

static uint8_t *cache_storage()
{
    if (g_storage == nullptr)
    {
        g_storage = (uint8_t *)SRL::Memory::HighWorkRam::Malloc(FILE_CACHE_MAX);
    }

    return g_storage;
}

static DiscFile *g_cache     = nullptr;
static char       g_cacheName[32];
static bool       g_cacheBusy = false;

static bool normalize_name(const char *name, char *out, int32_t outSize)
{
    if (name == nullptr || out == nullptr || outSize < 2)
    {
        return false;
    }

    const char *base = name;

    for (const char *p = name; *p != '\0'; p++)
    {
        if (*p == '/' || *p == '\\')
        {
            base = p + 1;
        }
    }

    int32_t n = 0;
    bool hasDot = false;

    while (base[n] != '\0' && n < outSize - 2)
    {
        char c = base[n];

        if (c >= 'a' && c <= 'z')
        {
            c = (char)(c - 'a' + 'A');
        }

        if (c == '.')
        {
            hasDot = true;
        }

        out[n] = c;
        n++;
    }

    if (n == 0)
    {
        return false;
    }

    if (!hasDot)
    {
        out[n++] = '.';
    }

    out[n] = '\0';
    return true;
}

extern "C" DiscFile *disc_open(const char *name)
{
    char resolved[32];

    if (!normalize_name(name, resolved, (int32_t)sizeof(resolved)))
    {
        return nullptr;
    }

    if (g_cache != nullptr && !g_cacheBusy && strcmp(g_cacheName, resolved) == 0)
    {
        g_cacheBusy = true;
        return g_cache;
    }

    SRL::Cd::File *file = new SRL::Cd::File(resolved);

    if (file == nullptr)
    {
        return nullptr;
    }

    if (!file->Exists())
    {
        delete file;
        return nullptr;
    }

    DiscFile *handle = new DiscFile();

    if (handle == nullptr)
    {
        delete file;
        return nullptr;
    }

    handle->File = file;
    handle->Size = (int32_t)file->Size.Bytes;
    handle->Data = nullptr;

    if (handle->Size > 0 && handle->Size <= FILE_CACHE_MAX)
    {
        const int32_t rounded =
            (handle->Size + SECTOR_BYTES - 1) / SECTOR_BYTES * SECTOR_BYTES;

        uint8_t *data = g_cacheBusy ? nullptr : cache_storage();

        if (data != nullptr)
        {
            int32_t loaded = 0;
            bool    ok     = true;

            cd_music_suspend();

            while (loaded < rounded)
            {
                int32_t window = rounded - loaded;

                if (window > CACHE_WINDOW_BYTES)
                {
                    window = CACHE_WINDOW_BYTES;
                }

                int32_t got = file->LoadBytes((size_t)(loaded / SECTOR_BYTES),
                                               window, data + loaded);

                if (got < 0)
                {
                    ok = false;
                    break;
                }

                loaded += window;

                sat_loading_tick();
            }

            cd_music_restore();

            if (!ok)
            {
            }
            else
            {
                handle->Data = data;
            }

        }
    }

    if (!g_cacheBusy)
    {
        if (g_cache != nullptr)
        {
            delete g_cache->File;
            delete g_cache;
        }

        g_cache = handle;
        strncpy(g_cacheName, resolved, sizeof(g_cacheName) - 1);
        g_cacheName[sizeof(g_cacheName) - 1] = '\0';
        g_cacheBusy = true;
    }

    return handle;
}

extern "C" void disc_close(DiscFile *file)
{
    if (file == nullptr)
    {
        return;
    }

    if (file == g_cache)
    {
        g_cacheBusy = false;
        return;
    }

    delete file->File;
    delete file;
}

extern "C" int32_t disc_size(DiscFile *file)
{
    return file != nullptr ? file->Size : -1;
}

extern "C" int32_t disc_read(DiscFile *file, int32_t pos, void *dst, int32_t size)
{
    if (file == nullptr || dst == nullptr || size <= 0)
    {
        return -1;
    }

    if (pos < 0 || pos >= file->Size)
    {
        return -1;
    }

    if (pos + size > file->Size)
    {
        size = file->Size - pos;
    }

    if (file->Data != nullptr)
    {
        memcpy(dst, file->Data + pos, (size_t)size);
        return size;
    }

    uint8_t *bounce = bounce_buffer();

    if (bounce == nullptr)
    {
        return -1;
    }

    uint8_t *out = (uint8_t *)dst;
    int32_t done = 0;

    cd_music_suspend();

    while (done < size)
    {
        const int32_t at     = pos + done;
        const int32_t sector = at / SECTOR_BYTES;
        const int32_t skew   = at % SECTOR_BYTES;

        int32_t want = size - done;

        if (want > BOUNCE_BYTES - skew)
        {
            want = BOUNCE_BYTES - skew;
        }

        int32_t bytes = ((skew + want) + SECTOR_BYTES - 1) / SECTOR_BYTES * SECTOR_BYTES;
        const int32_t available = file->Size - (sector * SECTOR_BYTES);

        if (bytes > available)
        {
            bytes = available;
        }

        int32_t got = file->File->LoadBytes((size_t)sector, bytes, bounce);

        if (got < 0)
        {
            cd_music_restore();
            return done > 0 ? done : -1;
        }

        memcpy(out + done, bounce + skew, want);
        done += want;

        sat_loading_tick();
    }

    cd_music_restore();

    return done;
}
