#include <stdint.h>
#include "cd_music.h"
#include "cd_drive.h"
#include "clock.h"
#include "diag.h"
#include "part_music.h"
#include "cdda_arm.h"
#include "cdda_classify.h"
#include "cdda_gate.h"
#include "cdda_want.h"
#include "cdtoc.h"

#define CDDA_DIAG 0

#if CDDA_DIAG
#undef printf
extern "C" int printf(const char *fmt, ...);
#define CDDA_SAY(...) printf(__VA_ARGS__)
#else
#define CDDA_SAY(...) ((void)0)
#endif

#define CDDA_PROBE_DIAG 0

#if CDDA_PROBE_DIAG
#define CDDA_ROW(...) diag_row(__VA_ARGS__)
#else
#define CDDA_ROW(...) ((void)0)
#endif

static uint32_t g_toc[CDTOC_WORDS];
static int      g_tocValid    = 0;
static int      g_maxAudio    = 0;

static int g_cue  = 0;
static int g_loop = 0;

static int g_wantCue = 0;

static bool g_observed = false;

static bool     g_wasPlaying = false;
static uint32_t g_pauseFad   = 0;

static int g_paused = 0;

static uint32_t g_skipFads = 0;

static int g_tailSeen = 0;

#define CDDA_FADS_PER_SECOND 75u

extern "C" {
static void cdda_set_cue_skipped(int cue, int loop, unsigned int delayMs,
                                 unsigned int skipMs);
static void cdda_set_part_at(unsigned short partId, unsigned int skipMs);
static void cdda_play_from_top(void);
}

static int g_armPending = 0;
static int g_idleFrames = 0;
static int g_loopTail   = 0;

static bool g_liveAudible = false;

static int g_stallFrames = 0;

static int32_t g_playedMs = -1;

static int g_restarts = 0;

#define CDDA_STALL_FRAMES 30

static uint32_t g_fadBase    = 0;
static int      g_fadBaseSet = 0;

#define CDDA_AUDIBLE_SECTORS 4u

static int g_volProbe = 0;

static uint32_t g_startAt = 0;
static uint32_t g_startGap = CDDA_GAP_NONE;

static uint32_t g_askedAt = 0;

static bool g_frameShown = false;

static bool g_noWait = false;

static uint32_t       g_fallbackAt   = 0;
static unsigned short g_fallbackPart = 0;

extern "C" {

static void cdda_halt(void)
{
    cd_drive_halt();
}

static void cdda_capture_and_halt(void)
{
    uint32_t fad;

    if (g_cue == 0 || g_paused)
    {
        return;
    }

    g_wasPlaying = cd_drive_status(&fad) != 0;
    g_pauseFad = fad;

    cdda_halt();
}

static void cdda_reinstate(void)
{
    uint32_t start;
    uint32_t end;
    cdda_action action;

    if (g_cue == 0 || g_paused)
    {
        return;
    }

    start = cdtoc_track_start(g_toc, g_cue);
    end = cdtoc_track_end(g_toc, g_cue);

    if (g_wasPlaying && end != 0 && g_pauseFad >= start && g_pauseFad < end)
    {
        g_observed = true;
    }

    action = cdda_classify(g_wasPlaying ? 1 : 0, g_loop,
        g_observed ? 1 : 0, g_pauseFad, start, end);

    CDDA_SAY("cdda: verdict %d for %d (was %d obs %d fad %d in %d..%d)\n",
             (int)action, g_cue, g_wasPlaying ? 1 : 0, g_observed ? 1 : 0,
             (int)g_pauseFad, (int)start, (int)end);

    switch (action)
    {
    case CDDA_FORGET:
        g_cue = 0;
        g_loopTail = 0;
        return;

    case CDDA_RESUME:
    case CDDA_RESUME_LOOP:
    {
        cd_drive_play_fad(g_pauseFad, end - g_pauseFad);
        g_loopTail = (action == CDDA_RESUME_LOOP) ? 1 : 0;
        g_tailSeen = 0;
        g_liveAudible = false;
        g_fadBaseSet = 0;
        return;
    }

    case CDDA_RESTART:
    default:
        if (g_observed && end != 0 && g_pauseFad >= start && g_pauseFad < end)
        {
            CDDA_SAY("cdda: %d resumed at %d rather than restarted\n", g_cue,
                     (int)(g_pauseFad - start));

            cd_drive_play_fad(g_pauseFad, end - g_pauseFad);

            g_loopTail = (g_loop != 0) ? 1 : 0;
            g_tailSeen = 0;
            g_liveAudible = false;
            g_fadBaseSet = 0;
            return;
        }

        if (!cdtoc_is_audio(g_toc, g_cue))
        {
            g_cue = 0;
            g_loopTail = 0;
            return;
        }

        CDDA_SAY("cdda: play %d loop %d held %d ms\n", g_cue, g_loop,
                 (int)(clock_ms() - g_askedAt));
        g_restarts++;
        cdda_play_from_top();
        return;
    }
}

void cd_music_init(void)
{
    if (g_tocValid)
    {
        return;
    }

    cd_drive_read_toc(g_toc);
    g_maxAudio = cdtoc_max_audio_track(g_toc);
    g_tocValid = 1;

    cd_drive_start_analysis();

    CDDA_SAY("cdda: toc max audio track %d, volume analysis started\n",
             g_maxAudio);
}

static void cdda_set_part_at(unsigned short partId, unsigned int skipMs)
{
    cdda_set_cue_skipped((int)partMusicCue(partId), partMusicLoops(partId),
                         cdda_delay_for_pos(0), skipMs);
}

void cd_music_set_part(unsigned short partId)
{
    cdda_set_part_at(partId, 0u);
}

void cd_music_set_part_resumed(unsigned short partId)
{
    cdda_set_part_at(partId, partMusicResumeSkipMs(partMusicCue(partId)));
}

void cd_music_set_cue(int cue, int loop, unsigned int delayMs)
{
    cdda_set_cue_skipped(cue, loop, delayMs, 0u);
}

static void cdda_set_cue_skipped(int cue, int loop, unsigned int delayMs,
                                 unsigned int skipMs)
{
    const int      hadCue = g_cue;
    cdda_want_action action;

    g_fallbackAt = 0;

    if (!g_tocValid || cue > g_maxAudio || !cdtoc_is_audio(g_toc, cue))
    {
        cue = 0;
        loop = 0;
    }

    action = cdda_want(cue, g_wantCue, g_cue);

    CDDA_ROW(23, "cdda: ask%d had%d act%d aud%d", cue, hadCue, (int)action,
             g_liveAudible ? 1 : 0, 0);

    if (action == CDDA_WANT_KEEP)
    {
        return;
    }

    CDDA_SAY("cdda: want %d loop %d (had %d)\n", cue, loop, hadCue);

    g_wantCue = cue;
    g_paused = 0;

    g_armPending = 0;
    g_idleFrames = 0;
    g_loopTail = 0;

    if (action == CDDA_WANT_PLAY)
    {
        g_cue = cue;
        g_loop = loop;
        g_skipFads = (uint32_t)((skipMs * CDDA_FADS_PER_SECOND) / 1000u);
        g_observed = false;
        g_liveAudible = false;
        g_fadBaseSet = 0;
        g_wasPlaying = false;
        g_pauseFad = 0;
        g_restarts = 0;

        if (hadCue != 0)
        {
            cdda_halt();
        }

        g_askedAt = clock_ms();
        g_frameShown = false;

        if (delayMs == 0)
        {
            g_startGap = CDDA_GAP_NONE;
            g_startAt = g_askedAt;
            g_noWait = true;

            if (g_startAt == 0)
            {
                g_startAt = 1;
            }
        }
        else
        {
            g_startAt = 0;
            g_startGap = (uint32_t)delayMs;
            g_noWait = false;
        }

        return;
    }

    g_startAt = 0;
    g_startGap = CDDA_GAP_NONE;

    if (action == CDDA_WANT_STOP && cdda_gate_depth() == 0)
    {
        cdda_halt();
    }

    g_cue = 0;
    g_loop = 0;
    g_observed = false;
    g_liveAudible = false;
    g_fadBaseSet = 0;
}

void cd_music_stop(void)
{
    if (g_cue != 0)
    {
        cdda_halt();
    }

    g_cue = 0;
    g_loop = 0;
    g_wantCue = 0;
    g_observed = false;
    g_paused = 0;
    g_armPending = 0;
    g_idleFrames = 0;
    g_loopTail = 0;
    g_liveAudible = false;
    g_fadBaseSet = 0;
    g_startAt = 0;
    g_startGap = CDDA_GAP_NONE;
}

void cd_music_suspend(void)
{
    if (!cdda_gate_enter())
    {
        return;
    }

    g_idleFrames = 0;
    cdda_capture_and_halt();

    if (g_cue != 0 && !g_paused)
    {
        g_armPending = 1;
        g_loopTail = 0;
    }
}

void cd_music_restore(void)
{
    if (!cdda_gate_exit())
    {
        return;
    }

    g_idleFrames = 0;

    g_frameShown = false;
}

#define CDDA_IDLE_CAP 1000

static void cdda_observe(void)
{
    uint32_t start;
    uint32_t end;
    uint32_t fad;
    int      playing;

    if (g_cue == 0)
    {
        if (++g_volProbe >= 60)
        {
            g_volProbe = 0;
            CDDA_ROW(20, "cdda: cue0 -- forgotten", 0, 0, 0, 0, 0);
        }

        g_playedMs = -1;

        return;
    }

    playing = cd_drive_status(&fad);
    start = cdtoc_track_start(g_toc, g_cue);
    end = cdtoc_track_end(g_toc, g_cue);

    if (g_liveAudible && playing &&
        end != 0 && fad >= start && fad < end)
    {
        g_playedMs = (int32_t)(((fad - start) * 1000u) / 75u);
    }
    else
    {
        g_playedMs = -1;
    }

    if (++g_volProbe >= 60)
    {
        g_volProbe = 0;

        CDDA_ROW(20, "cdda: cue%d st%d fad%d-%d rst%d", (int)g_cue,
                 playing, (int)(fad - start),
                 (int)(end - start), g_restarts);

        if (!g_liveAudible)
        {
            const uint32_t probe = cd_drive_volume();

            (void)probe;

            CDDA_ROW(21, "cdda: vol%d play%d base%d adv%d aud%d",
                     (int)probe,
                     playing,
                     g_fadBaseSet, (int)(fad - g_fadBase),
                     g_liveAudible ? 1 : 0);
        }
    }

    if (cdda_gate_depth() != 0)
    {
        return;
    }

    if (!playing)
    {
        return;
    }

    if (end != 0 && (fad < start || fad >= end))
    {
        return;
    }

    if (!g_fadBaseSet || fad < g_fadBase)
    {
        g_fadBase = fad;
        g_fadBaseSet = 1;
        return;
    }

    if (fad - g_fadBase >= CDDA_AUDIBLE_SECTORS)
    {
        if (!g_liveAudible)
        {
            CDDA_ROW(21, "cdda: aud fad+%d ask+%d ms", (int)(fad - start),
                     (int)(clock_ms() - g_askedAt), 0, 0, 0);

            CDDA_SAY("cdda: %d audible %d ms after ask\n", g_cue,
                     (int)(clock_ms() - g_askedAt));
        }

        g_observed = true;
        g_liveAudible = true;
    }
}

static void cdda_play_from_top(void)
{
    const uint32_t start = cdtoc_track_start(g_toc, g_cue);
    const uint32_t end   = cdtoc_track_end(g_toc, g_cue);

    if (g_skipFads != 0 && end != 0 && start + g_skipFads < end)
    {
        CDDA_SAY("cdda: play %d from +%d fads, skipping its opening\n",
                 g_cue, (int)g_skipFads);

        cd_drive_play_fad(start + g_skipFads, end - (start + g_skipFads));

        g_loopTail = (g_loop != 0) ? 1 : 0;
        g_tailSeen = 0;
        g_liveAudible = false;
        g_fadBaseSet = 0;
        return;
    }

    cd_drive_play_track((uint16_t)g_cue, g_loop != 0);
    g_loopTail = 0;
    g_tailSeen = 0;
    g_liveAudible = false;
    g_fadBaseSet = 0;
}

static void cdda_watch(void)
{
    uint32_t start;
    uint32_t end;
    uint32_t fad;
    int      playing;

    cdda_observe();

    playing = cd_drive_status(&fad);

    start = cdtoc_track_start(g_toc, g_cue);
    end = cdtoc_track_end(g_toc, g_cue);

    if (fad != CD_DRIVE_FAD_ERR && playing && end != 0 &&
        fad >= start && fad < end)
    {
        g_tailSeen = 1;
    }

    if (g_tailSeen && fad != CD_DRIVE_FAD_ERR &&
        cdda_loop_expired(g_loopTail, playing, fad, end))
    {
        g_restarts++;
        cdda_play_from_top();
        return;
    }

    if (!g_liveAudible)
    {
        return;
    }

    if (!playing && !g_paused && cdda_gate_depth() == 0 &&
        end != 0 && fad >= start && fad < end)
    {
        if (++g_stallFrames >= CDDA_STALL_FRAMES)
        {
            CDDA_SAY("cdda: %d stalled at %d, putting it back\n", g_cue,
                     (int)(fad - start));
            CDDA_ROW(20, "cdda: cue%d STALLED fad%d -- rearming", (int)g_cue,
                     (int)(fad - start), 0, 0, 0);

            g_stallFrames = 0;
            g_wasPlaying = true;
            g_pauseFad = fad;
            g_armPending = 1;
        }

        return;
    }

    g_stallFrames = 0;

    if (cdda_classify(playing, g_loop, g_liveAudible ? 1 : 0, fad, start, end)
        == CDDA_FORGET)
    {
        CDDA_SAY("cdda: %d finished\n", g_cue);
        g_cue = 0;
        g_loopTail = 0;
    }
}

void cd_music_frame_shown(void)
{
    g_frameShown = true;
}

void cd_music_tick(void)
{
    if (g_paused)
    {
        return;
    }

    if (g_fallbackAt != 0 &&
        (int32_t)(clock_ms() - g_fallbackAt) >= 0)
    {
        const unsigned short part = g_fallbackPart;

        g_fallbackAt = 0;
        cd_music_set_part(part);
    }

    cdda_observe();

    if (g_cue == 0)
    {
        return;
    }

    if (g_idleFrames < CDDA_IDLE_CAP)
    {
        g_idleFrames++;
    }

    if (g_startGap != CDDA_GAP_NONE)
    {
        if (!g_frameShown ||
            cdda_arm(1, cdda_gate_depth(), g_idleFrames,
                     CDDA_ARM_SETTLE_FRAMES) != CDDA_ARM_NOW)
        {
            return;
        }

        g_startAt = clock_ms() + g_startGap;

        if (g_startAt == 0)
        {
            g_startAt = 1;
        }

        g_startGap = CDDA_GAP_NONE;
        CDDA_SAY("cdda: gate open after %d ms, gap starts for %d\n",
                 (int)(clock_ms() - g_askedAt), g_cue);
    }

    if (g_startAt != 0)
    {
        if ((int32_t)(clock_ms() - g_startAt) < 0)
        {
            return;
        }

        g_startAt = 0;
        g_armPending = 1;
        CDDA_SAY("cdda: gap over, arming %d\n", g_cue);
    }

    if (g_armPending)
    {
        if (cdda_arm(1, cdda_gate_depth(), g_idleFrames,
                     g_noWait ? 0 : CDDA_ARM_SETTLE_FRAMES) == CDDA_ARM_NOW)
        {
            g_armPending = 0;
            g_noWait = false;
            cdda_reinstate();
        }

        return;
    }

    cdda_watch();
}

void cd_music_pause(void)
{
    if (g_paused)
    {
        return;
    }

    cdda_capture_and_halt();
    g_paused = 1;
    g_armPending = 0;
    g_loopTail = 0;
    g_liveAudible = false;
    g_fadBaseSet = 0;
}

void cd_music_resume(void)
{
    if (!g_paused)
    {
        return;
    }

    g_paused = 0;
    g_armPending = 0;
    g_idleFrames = 0;
    cdda_reinstate();
}

int cd_music_is_playing(void)
{
    return g_cue != 0;
}

int cd_music_audible(void)
{
    return g_liveAudible ? 1 : 0;
}

int cd_music_track_ms(void)
{
    return (int)g_playedMs;
}

unsigned int cd_music_ms_since_cue(void)
{
    if (g_cue == 0)
    {
        return 0u;
    }

    return (unsigned int)(clock_ms() - g_askedAt);
}

int cd_music_available(void)
{
    return g_maxAudio != 0;
}

void cd_music_part_began(unsigned short partId)
{
    g_fallbackPart = partId;
    g_fallbackAt = clock_ms() + CDDA_FALLBACK_MS;

    if (g_fallbackAt == 0)
    {
        g_fallbackAt = 1;
    }
}

}
