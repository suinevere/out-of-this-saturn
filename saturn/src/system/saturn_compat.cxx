#include <srl.hpp>
#include "compat.h"

static bool inHighWorkRam(void *ptr)
{
    const uint32_t address = ((uint32_t)ptr) & 0x0FFFFFFF;
    return address >= 0x06000000;
}

extern "C" void *malloc(size_t size)
{
    void *ptr = SRL::Memory::HighWorkRam::Malloc(size);

    if (ptr == nullptr) {
        ptr = SRL::Memory::LowWorkRam::Malloc(size);
    }

    return ptr;
}

extern "C" void free(void *ptr)
{
    if (ptr != nullptr) {
        if (inHighWorkRam(ptr)) {
            SRL::Memory::HighWorkRam::Free(ptr);
        } else {
            SRL::Memory::LowWorkRam::Free(ptr);
        }
    }
}

extern "C" void *mem_alloc_low(size_t size)
{
    return SRL::Memory::LowWorkRam::Malloc(size);
}

extern "C" void *realloc(void *ptr, size_t size)
{
    if (ptr == nullptr) {
        return malloc(size);
    }

    if (inHighWorkRam(ptr)) {
        return SRL::Memory::HighWorkRam::Realloc(ptr, size);
    }

    return SRL::Memory::LowWorkRam::Realloc(ptr, size);
}

static int g_stream_tags[2];
extern "C" FILE *stdout = (FILE *)&g_stream_tags[0];
extern "C" FILE *stderr = (FILE *)&g_stream_tags[1];

extern "C" int vsnprintf(char *str, size_t size, const char *fmt, __builtin_va_list ap);

#define DIAG_ROWS 27
#define DIAG_COLS 40
static int g_diagRow = 0;

static void diag_emit(const char *fmt, __builtin_va_list ap)
{
    char line[DIAG_COLS + 1];
    vsnprintf(line, sizeof(line), fmt, ap);

    for (int i = 0; line[i] != '\0'; i++)
    {
        if (line[i] == '\n' || line[i] == '\r')
        {
            line[i] = '\0';
            break;
        }
    }

    SRL::Debug::Print(0, g_diagRow, "%s", line);
    g_diagRow++;

    if (g_diagRow >= DIAG_ROWS)
    {
        g_diagRow = 0;
    }
}

extern "C" int printf(const char *fmt, ...)
{
#if SATURN_DIAG
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    diag_emit(fmt, ap);
    __builtin_va_end(ap);
#else
    (void)fmt;
#endif
    return 0;
}

extern "C" int fprintf(FILE *stream, const char *fmt, ...)
{
    (void)stream;
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    diag_emit(fmt, ap);
    __builtin_va_end(ap);
    return 0;
}

extern "C" void exit(int status)
{
    (void)status;
    while (true) {
        SRL::Core::Synchronize();
    }
}
