#include <cstdio>
#include <cstdint>
#include "scsp_voice.h"

static int g_fail = 0;

#define CHECK_EQ(actual, expected)                                            \
    do {                                                                      \
        long long a_ = (long long)(actual);                                   \
        long long e_ = (long long)(expected);                                 \
        if (a_ != e_) {                                                       \
            g_fail++;                                                         \
            printf("FAIL %s:%d  %s\n  actual   = %lld (0x%llX)\n"             \
                   "  expected = %lld (0x%llX)\n",                            \
                   __FILE__, __LINE__, #actual, a_,                           \
                   (unsigned long long)a_, e_, (unsigned long long)e_);       \
        }                                                                     \
    } while (0)

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            g_fail++;                                                         \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);            \
        }                                                                     \
    } while (0)

static int pitchOct(uint16_t w) { int o = (w >> 11) & 0xF; return (o & 0x8) ? o - 16 : o; }
static int pitchFns(uint16_t w) { return w & 0x3FF; }

static void test_pitch_known_rates(void)
{
    CHECK_EQ(scsp_voice_pitch(44100), 0x0000);

    CHECK_EQ(scsp_voice_pitch(22050), 0x7800);
    CHECK_EQ(scsp_voice_pitch(11025), 0x7000);
    CHECK_EQ(scsp_voice_pitch(5512),  0x6800);

    CHECK_EQ(scsp_voice_pitch(33075), 0x7A00);

    CHECK_EQ(scsp_voice_pitch(16537), 0x71FF);

    CHECK_EQ(scsp_voice_pitch(65082), 0x01E7);

    CHECK_EQ(scsp_voice_pitch(874), 0x5112);
}

static void test_pitch_msk10_overflow(void)
{
    CHECK_EQ(scsp_voice_pitch(11024), 0x7000);
}

static void test_pitch_fields_stay_in_range(void)
{
    for (uint32_t rate = 874; rate <= 65082; rate++) {
        uint16_t w = scsp_voice_pitch(rate);
        int oct = pitchOct(w);
        int fns = pitchFns(w);
        if (oct < -8 || oct > 7 || fns < 0 || fns > 1023) {
            printf("FAIL rate %u -> 0x%04X OCT=%d FNS=%d out of range\n",
                   (unsigned)rate, w, oct, fns);
            g_fail++;
            return;
        }
    }
}

static void test_pitch_zero_is_safe(void)
{
    CHECK_EQ(scsp_voice_pitch(0), 0x0000);
}

static void test_tl_table(void)
{
    CHECK_EQ(scsp_voice_tl(63), 0);

    CHECK_EQ(scsp_voice_tl(32), 16);
    CHECK_EQ(scsp_voice_tl(16), 32);
    CHECK_EQ(scsp_voice_tl(8),  48);
    CHECK_EQ(scsp_voice_tl(4),  64);
    CHECK_EQ(scsp_voice_tl(2),  80);
    CHECK_EQ(scsp_voice_tl(1),  96);

    CHECK_EQ(scsp_voice_tl(0), 255);

    for (uint8_t v = 1; v < 63; v++) {
        CHECK(scsp_voice_tl(v) >= scsp_voice_tl((uint8_t)(v + 1)));
    }

    CHECK_EQ(scsp_voice_tl(64),  0);
    CHECK_EQ(scsp_voice_tl(255), 0);
}

static void test_upload_bytes(void)
{
    CHECK_EQ(scsp_voice_upload_bytes(1000, 0),   1000);
    CHECK_EQ(scsp_voice_upload_bytes(1000, 500), 1500);

    CHECK_EQ(scsp_voice_upload_bytes(999, 0),    1000);
    CHECK_EQ(scsp_voice_upload_bytes(999, 501),  1500);

    CHECK_EQ(scsp_voice_upload_bytes(60000, 60000), 120000);

    CHECK_EQ(scsp_voice_upload_bytes(0, 0), 0);
}

static void test_points_one_shot(void)
{
    ScspVoicePoints p;
    CHECK_EQ(scsp_voice_points(0x20000, 1000, 0, 0, &p), 1);
    CHECK_EQ(p.sa,   0x20000);
    CHECK_EQ(p.lsa,  0);
    CHECK_EQ(p.lea,  999);
    CHECK_EQ(p.loop, 0);
}

static void test_points_looping(void)
{
    ScspVoicePoints p;
    CHECK_EQ(scsp_voice_points(0x20000, 1000, 1000, 500, &p), 1);
    CHECK_EQ(p.sa,   0x20000);
    CHECK_EQ(p.lsa,  1000);
    CHECK_EQ(p.lea,  1499);
    CHECK_EQ(p.loop, 1);
}

static void test_points_rejects_bad_input(void)
{
    ScspVoicePoints p;

    CHECK_EQ(scsp_voice_points(0x20000, 0, 0, 0, &p), 0);

    CHECK_EQ(scsp_voice_points(0x20000, 100, 200, 50, &p), 0);

    CHECK_EQ(scsp_voice_points(0x20000, 60000, 60000, 60000, &p), 0);
}

static const uint8_t *fakeData(unsigned n)
{
    static const uint8_t blob[64] = { 0 };
    return blob + (n & 63);
}

static void test_cache_miss_then_hit(void)
{
    ScspCache c;
    uint32_t off = 0;

    scsp_cache_init(&c, 0x20000, 0x80000);

    CHECK_EQ(scsp_cache_acquire(&c, fakeData(1), 1000, 0, &off), SCSP_CACHE_MISS);
    CHECK_EQ(off, 0x20000);

    CHECK_EQ(scsp_cache_acquire(&c, fakeData(1), 1000, 0, &off), SCSP_CACHE_HIT);
    CHECK_EQ(off, 0x20000);

    CHECK_EQ(scsp_cache_acquire(&c, fakeData(2), 500, 0, &off), SCSP_CACHE_MISS);
    CHECK_EQ(off, 0x20000 + 1000);

    CHECK_EQ(scsp_cache_used_entries(&c), 2);
    CHECK_EQ(scsp_cache_used_bytes(&c), 1500);
}

static void test_cache_same_pointer_different_geometry_is_a_miss(void)
{
    ScspCache c;
    uint32_t off = 0;

    scsp_cache_init(&c, 0x20000, 0x80000);

    CHECK_EQ(scsp_cache_acquire(&c, fakeData(1), 1000, 0, &off), SCSP_CACHE_MISS);
    CHECK_EQ(scsp_cache_acquire(&c, fakeData(1), 2000, 0, &off), SCSP_CACHE_MISS);
    CHECK_EQ(off, 0x20000 + 1000);
}

static void test_cache_reset_forgets_everything(void)
{
    ScspCache c;
    uint32_t off = 0;

    scsp_cache_init(&c, 0x20000, 0x80000);
    CHECK_EQ(scsp_cache_acquire(&c, fakeData(1), 1000, 0, &off), SCSP_CACHE_MISS);

    scsp_cache_reset(&c);

    CHECK_EQ(scsp_cache_used_entries(&c), 0);
    CHECK_EQ(scsp_cache_used_bytes(&c), 0);
    CHECK_EQ(scsp_cache_acquire(&c, fakeData(1), 1000, 0, &off), SCSP_CACHE_MISS);
    CHECK_EQ(off, 0x20000);
}

static void test_cache_exhaustion_resets_and_reports_it(void)
{
    ScspCache c;
    uint32_t off = 0;

    scsp_cache_init(&c, 0x20000, 0x21000);

    CHECK_EQ(scsp_cache_acquire(&c, fakeData(1), 3000, 0, &off), SCSP_CACHE_MISS);
    CHECK_EQ(off, 0x20000);

    CHECK_EQ(scsp_cache_acquire(&c, fakeData(2), 2000, 0, &off),
             SCSP_CACHE_MISS_AFTER_RESET);
    CHECK_EQ(off, 0x20000);
    CHECK_EQ(scsp_cache_used_entries(&c), 1);
}

static void test_cache_too_big_is_refused(void)
{
    ScspCache c;
    uint32_t off = 0;

    scsp_cache_init(&c, 0x20000, 0x21000);

    CHECK_EQ(scsp_cache_acquire(&c, fakeData(1), 5000, 0, &off), SCSP_CACHE_TOO_BIG);

    CHECK_EQ(scsp_cache_acquire(&c, fakeData(2), 0, 0, &off), SCSP_CACHE_TOO_BIG);
}

static void test_cache_full_table_resets(void)
{
    ScspCache c;
    uint32_t off = 0;

    scsp_cache_init(&c, 0x20000, 0x80000);

    for (unsigned i = 0; i < SCSP_CACHE_ENTRIES; i++) {
        CHECK_EQ(scsp_cache_acquire(&c, fakeData(i), 16, 0, &off), SCSP_CACHE_MISS);
    }
    CHECK_EQ(scsp_cache_used_entries(&c), SCSP_CACHE_ENTRIES);

    CHECK_EQ(scsp_cache_acquire(&c, fakeData(SCSP_CACHE_ENTRIES), 16, 0, &off),
             SCSP_CACHE_MISS_AFTER_RESET);
    CHECK_EQ(scsp_cache_used_entries(&c), 1);
    CHECK_EQ(off, 0x20000);
}

int main(void)
{
    test_pitch_known_rates();
    test_pitch_msk10_overflow();
    test_pitch_fields_stay_in_range();
    test_pitch_zero_is_safe();
    test_tl_table();
    test_upload_bytes();
    test_points_one_shot();
    test_points_looping();
    test_points_rejects_bad_input();
    test_cache_miss_then_hit();
    test_cache_same_pointer_different_geometry_is_a_miss();
    test_cache_reset_forgets_everything();
    test_cache_exhaustion_resets_and_reports_it();
    test_cache_too_big_is_refused();
    test_cache_full_table_resets();

    if (g_fail == 0) {
        printf("all tests passed\n");
        return 0;
    }

    printf("%d check(s) failed\n", g_fail);
    return 1;
}
