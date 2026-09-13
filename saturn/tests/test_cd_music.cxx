#include <cstdio>
#include <cstdint>
#include "cd_music.h"
#include "cd_drive.h"
#include "clock.h"
#include "diag.h"
extern "C" {
#include "cdtoc.h"
}

static int g_fail = 0;

#define CHECK_EQ(actual, expected)                                            \
    do {                                                                      \
        long long a_ = (long long)(actual);                                   \
        long long e_ = (long long)(expected);                                 \
        if (a_ != e_) {                                                       \
            g_fail++;                                                         \
            printf("FAIL %s:%d  %s\n  actual   = %lld\n  expected = %lld\n",  \
                   __FILE__, __LINE__, #actual, a_, e_);                      \
        }                                                                     \
    } while (0)

static uint32_t g_toc[CDTOC_WORDS];
static uint32_t g_now = 1000;
static int g_halts = 0;
static int g_playedTrack = 0;
static int g_playedLoop = -1;
static int g_playing = 0;

extern "C" {

uint32_t clock_ms(void) { return g_now; }

void diag_row(int, const char *, int, int, int, int, int) {}

void cd_drive_read_toc(uint32_t *toc)
{
    for (int i = 0; i < CDTOC_WORDS; i++) {
        toc[i] = g_toc[i];
    }
}

void cd_drive_start_analysis(void) {}

void cd_drive_halt(void)
{
    g_halts++;
    g_playing = 0;
}

int cd_drive_status(uint32_t *fad)
{
    *fad = CD_DRIVE_FAD_ERR;
    return g_playing;
}

void cd_drive_play_fad(uint32_t, uint32_t) { g_playing = 1; }

void cd_drive_play_track(uint16_t track, int loop)
{
    g_playedTrack = track;
    g_playedLoop = loop;
    g_playing = 1;
}

uint32_t cd_drive_volume(void) { return 0; }

}

static uint32_t entry(uint32_t ctrl, uint32_t fad)
{
    return (ctrl << 28) | (fad & 0x00FFFFFFu);
}

static void run_ticks(int n)
{
    for (int i = 0; i < n; i++) {
        g_now += 17;
        cd_music_frame_shown();
        cd_music_tick();
    }
}

int main(void)
{
    for (int i = 0; i < CDTOC_WORDS; i++) {
        g_toc[i] = 0xFFFFFFFFu;
    }
    g_toc[0] = entry(0x4, 150);
    g_toc[1] = entry(0x0, 1000);
    g_toc[2] = entry(0x0, 2000);
    g_toc[CDTOC_FIRST_WORD]   = (0x4u << 28) | (1u << 16);
    g_toc[CDTOC_LAST_WORD]    = (0x0u << 28) | (3u << 16);
    g_toc[CDTOC_LEADOUT_WORD] = entry(0x0, 3000);

    cd_music_init();
    CHECK_EQ(cd_music_available(), 1);

    cd_music_set_cue(2, 1, 0);
    CHECK_EQ(cd_music_is_playing(), 1);
    run_ticks(10);
    CHECK_EQ(g_playedTrack, 2);
    CHECK_EQ(g_playedLoop, 1);

    const int haltsBefore = g_halts;
    cd_music_stop();
    CHECK_EQ(g_halts, haltsBefore + 1);
    CHECK_EQ(cd_music_is_playing(), 0);

    g_playedTrack = 0;
    cd_music_set_cue(9, 1, 0);
    run_ticks(10);
    CHECK_EQ(g_playedTrack, 0);
    CHECK_EQ(cd_music_is_playing(), 0);

    if (g_fail) {
        printf("cd_music: %d failure(s)\n", g_fail);
        return 1;
    }
    printf("cd_music: all passed\n");
    return 0;
}
