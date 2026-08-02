#include <cstdio>
#include <cstring>
#include <cstdint>
#include "keymap.h"

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

static void test_bits_round_trip(void)
{
    for (int i = PAD_NONE + 1; i < PAD_COUNT; ++i) {
        const uint32_t bit = keymapButtonBit((PadButton)i);
        CHECK(bit != 0);
        CHECK_EQ(keymapButtonFromBit(bit), i);
    }

    CHECK_EQ(keymapButtonBit(PAD_NONE), 0);
    CHECK_EQ(keymapButtonFromBit(0), PAD_NONE);
    CHECK_EQ(keymapButtonFromBit(PAD_BIT_UP), PAD_NONE);
    CHECK_EQ(keymapButtonFromBit(PAD_BIT_PAUSE), PAD_NONE);
}

static void test_bits_are_distinct(void)
{
    uint32_t seen = 0;

    for (int i = PAD_NONE + 1; i < PAD_COUNT; ++i) {
        const uint32_t bit = keymapButtonBit((PadButton)i);
        CHECK_EQ(seen & bit, 0);
        seen |= bit;
    }
}

static void test_defaults_are_valid_and_the_shipping_layout(void)
{
    KeyMap m;
    keymapDefaults(&m);

    CHECK(keymapValid(&m));
    CHECK_EQ(m.row[KEYMAP_ROW_ACTION], PAD_A);
    CHECK_EQ(m.row[KEYMAP_ROW_JUMP], PAD_C);
    CHECK_EQ(m.row[KEYMAP_ROW_RUN], PAD_B);
}

static void test_assign_swaps_rather_than_unbinding(void)
{
    KeyMap m;
    keymapDefaults(&m);

    CHECK(keymapAssign(&m, KEYMAP_ROW_ACTION, PAD_C));
    CHECK_EQ(m.row[KEYMAP_ROW_ACTION], PAD_C);
    CHECK_EQ(m.row[KEYMAP_ROW_JUMP], PAD_A);
    CHECK_EQ(m.row[KEYMAP_ROW_RUN], PAD_B);
    CHECK(keymapValid(&m));
}

static void test_assign_of_an_unheld_button_displaces_nothing(void)
{
    KeyMap m;
    keymapDefaults(&m);

    CHECK(keymapAssign(&m, KEYMAP_ROW_RUN, PAD_Z));
    CHECK_EQ(m.row[KEYMAP_ROW_ACTION], PAD_A);
    CHECK_EQ(m.row[KEYMAP_ROW_JUMP], PAD_C);
    CHECK_EQ(m.row[KEYMAP_ROW_RUN], PAD_Z);
    CHECK(keymapValid(&m));
}

static void test_assign_is_a_no_op_when_nothing_changes(void)
{
    KeyMap m;
    keymapDefaults(&m);

    CHECK(!keymapAssign(&m, KEYMAP_ROW_ACTION, PAD_A));
    CHECK(!keymapAssign(&m, KEYMAP_ROW_ACTION, PAD_NONE));
    CHECK_EQ(m.row[KEYMAP_ROW_ACTION], PAD_A);
    CHECK(keymapValid(&m));
}

static void test_every_assignment_leaves_the_map_valid(void)
{
    for (int row = 0; row < KEYMAP_ROW_COUNT; ++row) {
        for (int b = PAD_NONE + 1; b < PAD_COUNT; ++b) {
            KeyMap m;
            keymapDefaults(&m);
            keymapAssign(&m, (KeymapRow)row, (PadButton)b);
            CHECK(keymapValid(&m));
            CHECK_EQ(m.row[row], b);
        }
    }
}

static void test_valid_rejects_repeats_and_holes(void)
{
    KeyMap m;

    keymapDefaults(&m);
    m.row[KEYMAP_ROW_JUMP] = m.row[KEYMAP_ROW_ACTION];
    CHECK(!keymapValid(&m));

    keymapDefaults(&m);
    m.row[KEYMAP_ROW_RUN] = PAD_NONE;
    CHECK(!keymapValid(&m));

    keymapDefaults(&m);
    m.row[KEYMAP_ROW_RUN] = (PadButton)PAD_COUNT;
    CHECK(!keymapValid(&m));
}

static void test_apply_reads_each_row_independently(void)
{
    KeyMap m;
    bool action = true, jump = true, run = true;

    keymapDefaults(&m);

    keymapApply(&m, 0, &action, &jump, &run);
    CHECK(!action);
    CHECK(!jump);
    CHECK(!run);

    keymapApply(&m, PAD_BIT_A, &action, &jump, &run);
    CHECK(action);
    CHECK(!jump);
    CHECK(!run);

    keymapApply(&m, PAD_BIT_B | PAD_BIT_C, &action, &jump, &run);
    CHECK(!action);
    CHECK(jump);
    CHECK(run);
}

static void test_apply_follows_a_rebind(void)
{
    KeyMap m;
    bool action = false, jump = false, run = false;

    keymapDefaults(&m);
    keymapAssign(&m, KEYMAP_ROW_JUMP, PAD_X);

    keymapApply(&m, PAD_BIT_X, &action, &jump, &run);
    CHECK(jump);
    CHECK(!action);

    keymapApply(&m, PAD_BIT_C, &action, &jump, &run);
    CHECK(!action);
    CHECK(!jump);
    CHECK(!run);
}

static void test_apply_ignores_buttons_no_row_holds(void)
{
    KeyMap m;
    bool action = true, jump = true, run = true;

    keymapDefaults(&m);
    keymapApply(&m, PAD_BIT_L | PAD_BIT_R | PAD_BIT_PAUSE | PAD_BIT_UP,
                &action, &jump, &run);
    CHECK(!action);
    CHECK(!jump);
    CHECK(!run);
}

static void test_active_starts_at_the_defaults(void)
{
    KeyMap expect;
    keymapDefaults(&expect);

    const KeyMap *live = keymapActive();
    for (int i = 0; i < KEYMAP_ROW_COUNT; ++i) {
        CHECK_EQ(live->row[i], expect.row[i]);
    }
}

static void test_set_active_refuses_an_unusable_map(void)
{
    KeyMap good;
    KeyMap bad;

    keymapDefaults(&good);
    keymapAssign(&good, KEYMAP_ROW_RUN, PAD_Y);
    keymapSetActive(&good);
    CHECK_EQ(keymapActive()->row[KEYMAP_ROW_RUN], PAD_Y);

    keymapDefaults(&bad);
    bad.row[KEYMAP_ROW_JUMP] = bad.row[KEYMAP_ROW_ACTION];
    keymapSetActive(&bad);

    CHECK_EQ(keymapActive()->row[KEYMAP_ROW_RUN], PAD_Y);
    CHECK(keymapValid(keymapActive()));
}

static void test_names(void)
{
    CHECK(strcmp(keymapButtonName(PAD_A), "A") == 0);
    CHECK(strcmp(keymapButtonName(PAD_Z), "Z") == 0);
    CHECK(strcmp(keymapButtonName(PAD_NONE), "-") == 0);
    CHECK(strcmp(keymapRowName(KEYMAP_ROW_ACTION), "ACTION") == 0);
    CHECK(strcmp(keymapRowName(KEYMAP_ROW_JUMP), "JUMP") == 0);
    CHECK(strcmp(keymapRowName(KEYMAP_ROW_RUN), "RUN") == 0);
}

int main(void)
{
    test_bits_round_trip();
    test_bits_are_distinct();
    test_defaults_are_valid_and_the_shipping_layout();
    test_assign_swaps_rather_than_unbinding();
    test_assign_of_an_unheld_button_displaces_nothing();
    test_assign_is_a_no_op_when_nothing_changes();
    test_every_assignment_leaves_the_map_valid();
    test_valid_rejects_repeats_and_holes();
    test_apply_reads_each_row_independently();
    test_apply_follows_a_rebind();
    test_apply_ignores_buttons_no_row_holds();
    test_active_starts_at_the_defaults();
    test_set_active_refuses_an_unusable_map();
    test_names();

    if (g_fail == 0) {
        printf("keymap: all tests passed\n");
        return 0;
    }
    printf("keymap: %d failure(s)\n", g_fail);
    return 1;
}
