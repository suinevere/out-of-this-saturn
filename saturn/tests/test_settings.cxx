#include <cstdio>
#include <cstring>
#include <cstdint>
#include "settings.h"
#include "backup.h"

extern void stub_bup_reset(void);
extern void stub_bup_set_device(uint32_t device, int present, int formatted,
                                int writeProtected, uint32_t freeBytes);
extern void stub_bup_add_file(uint32_t device, const char *name,
                              const void *data, int32_t size, uint32_t date);

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

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            g_fail++;                                                         \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);            \
        }                                                                     \
    } while (0)

static void custom(Settings *s)
{
    s->map.row[KEYMAP_ROW_ACTION] = PAD_X;
    s->map.row[KEYMAP_ROW_JUMP]   = PAD_Y;
    s->map.row[KEYMAP_ROW_RUN]    = PAD_Z;
}

static bool sameMap(const KeyMap *a, const KeyMap *b)
{
    for (int i = 0; i < KEYMAP_ROW_COUNT; ++i) {
        if (a->row[i] != b->row[i]) {
            return false;
        }
    }
    return true;
}

static void test_defaults_are_the_shipping_layout(void)
{
    Settings s;
    custom(&s);
    settingsDefaults(&s);

    KeyMap expect;
    keymapDefaults(&expect);
    CHECK(sameMap(&s.map, &expect));
}

static void test_pack_unpack_round_trip(void)
{
    uint8_t buf[SETTINGS_SIZE];
    Settings in;
    Settings out;

    custom(&in);
    settingsPack(buf, &in);

    settingsDefaults(&out);
    CHECK(settingsUnpack(buf, &out));
    CHECK(sameMap(&out.map, &in.map));
}

static void test_unpack_rejects_a_bad_magic(void)
{
    uint8_t buf[SETTINGS_SIZE];
    Settings in;
    Settings out;

    custom(&in);
    settingsPack(buf, &in);
    buf[0] = 'X';

    settingsDefaults(&out);
    CHECK(!settingsUnpack(buf, &out));

    KeyMap expect;
    keymapDefaults(&expect);
    CHECK(sameMap(&out.map, &expect));
}

static void test_unpack_rejects_an_older_version(void)
{
    uint8_t buf[SETTINGS_SIZE];
    Settings in;
    Settings out;

    custom(&in);
    settingsPack(buf, &in);
    buf[5] = SETTINGS_VER - 1;

    settingsDefaults(&out);
    CHECK(!settingsUnpack(buf, &out));
}

static void test_unpack_rejects_a_mapping_with_a_repeat(void)
{
    uint8_t buf[SETTINGS_SIZE];
    Settings in;
    Settings out;

    custom(&in);
    settingsPack(buf, &in);
    buf[7] = buf[6];

    settingsDefaults(&out);
    CHECK(!settingsUnpack(buf, &out));
}

static void test_unpack_rejects_an_unbound_row(void)
{
    uint8_t buf[SETTINGS_SIZE];
    Settings in;
    Settings out;

    custom(&in);
    settingsPack(buf, &in);
    buf[8] = (uint8_t)PAD_NONE;

    settingsDefaults(&out);
    CHECK(!settingsUnpack(buf, &out));
}

static void test_pack_zeroes_reserved_bytes(void)
{
    uint8_t buf[SETTINGS_SIZE];
    Settings in;

    memset(buf, 0xFF, sizeof(buf));
    custom(&in);
    settingsPack(buf, &in);

    for (int i = 6 + KEYMAP_ROW_COUNT; i < SETTINGS_SIZE; ++i) {
        CHECK_EQ(buf[i], 0);
    }
}

static void test_load_with_no_record_leaves_caller_defaults(void)
{
    Settings s;

    stub_bup_reset();
    custom(&s);
    CHECK(!settingsLoad(&s));

    Settings expect;
    custom(&expect);
    CHECK(sameMap(&s.map, &expect.map));
}

static void test_store_then_load_round_trip(void)
{
    Settings in;
    Settings out;

    stub_bup_reset();
    custom(&in);
    CHECK_EQ(settingsStore(&in), BACKUP_OK);

    settingsDefaults(&out);
    CHECK(settingsLoad(&out));
    CHECK(sameMap(&out.map, &in.map));
}

static void test_store_overwrites_an_existing_record(void)
{
    Settings in;
    Settings out;

    stub_bup_reset();
    custom(&in);
    CHECK_EQ(settingsStore(&in), BACKUP_OK);

    settingsDefaults(&in);
    CHECK_EQ(settingsStore(&in), BACKUP_OK);

    custom(&out);
    CHECK(settingsLoad(&out));

    KeyMap expect;
    keymapDefaults(&expect);
    CHECK(sameMap(&out.map, &expect));
}

static void test_store_reports_a_missing_device(void)
{
    Settings in;

    stub_bup_reset();
    stub_bup_set_device(BACKUP_INTERNAL, 0, 0, 0, 0);
    custom(&in);
    CHECK_EQ(settingsStore(&in), BACKUP_ERR_NONE);
}

static void test_load_rejects_a_foreign_record(void)
{
    uint8_t junk[SETTINGS_SIZE];
    Settings s;

    stub_bup_reset();
    memset(junk, 0x5A, sizeof(junk));
    stub_bup_add_file(BACKUP_INTERNAL, "AW_CFG", junk, SETTINGS_SIZE, 0);

    custom(&s);
    CHECK(!settingsLoad(&s));

    Settings expect;
    custom(&expect);
    CHECK(sameMap(&s.map, &expect.map));
}

int main(void)
{
    test_defaults_are_the_shipping_layout();
    test_pack_unpack_round_trip();
    test_unpack_rejects_a_bad_magic();
    test_unpack_rejects_an_older_version();
    test_unpack_rejects_a_mapping_with_a_repeat();
    test_unpack_rejects_an_unbound_row();
    test_pack_zeroes_reserved_bytes();
    test_load_with_no_record_leaves_caller_defaults();
    test_store_then_load_round_trip();
    test_store_overwrites_an_existing_record();
    test_store_reports_a_missing_device();
    test_load_rejects_a_foreign_record();

    if (g_fail == 0) {
        printf("settings: all tests passed\n");
        return 0;
    }
    printf("settings: %d failure(s)\n", g_fail);
    return 1;
}
