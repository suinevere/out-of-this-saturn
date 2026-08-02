#include <cstdint>
#include "menu_state.h"
#include "checkpoints.h"

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

static MenuInput NONE;

static MenuInput press(const char *what)
{
    MenuInput in;
    memset(&in, 0, sizeof(in));
    in.captured = PAD_NONE;
    if (strcmp(what, "up") == 0) { in.up = true; in.dirEdge = MENU_DIR_UP; }
    if (strcmp(what, "down") == 0) { in.down = true; in.dirEdge = MENU_DIR_DOWN; }
    if (strcmp(what, "left") == 0) { in.left = true; in.dirEdge = MENU_DIR_LEFT; }
    if (strcmp(what, "right") == 0) { in.right = true; in.dirEdge = MENU_DIR_RIGHT; }
    if (strcmp(what, "confirm") == 0) in.confirm = true;
    if (strcmp(what, "cancel") == 0) in.cancel = true;
    if (strcmp(what, "pause") == 0) in.pause = true;
    return in;
}

static MenuInput repeated(const char *what)
{
    MenuInput in = press(what);
    in.dirEdge = 0;
    return in;
}

static MenuInput capture(PadButton b)
{
    MenuInput in;
    memset(&in, 0, sizeof(in));
    in.captured = b;
    return in;
}

static void freshTitle(MenuState *st, bool hasContinue)
{
    memset(st, 0, sizeof(*st));
    st->hasContinue = hasContinue;
    menuStateEnterTitle(st);
}

static MenuInput pressB()
{
    MenuInput in;
    memset(&in, 0, sizeof(in));
    in.cancel = true;
    in.captured = PAD_B;
    return in;
}

static MenuInput pressA()
{
    MenuInput in;
    memset(&in, 0, sizeof(in));
    in.confirm = true;
    in.captured = PAD_A;
    return in;
}

static MenuInput pressC()
{
    MenuInput in;
    memset(&in, 0, sizeof(in));
    in.confirm = true;
    in.captured = PAD_C;
    return in;
}

static MenuInput pressStart()
{
    MenuInput in;
    memset(&in, 0, sizeof(in));
    in.pause = true;
    return in;
}

static void enterKonami(MenuState *st, int steps)
{
    static const char *const SEQ[8] = {
        "up", "up", "down", "down", "left", "right", "left", "right"
    };

    for (int i = 0; i < steps && i < 8; ++i) {
        menuStateStep(st, &(NONE = press(SEQ[i])));
    }
    if (steps > 8) {
        menuStateStep(st, &(NONE = pressB()));
    }
    if (steps > 9) {
        menuStateStep(st, &(NONE = pressA()));
    }
    if (steps > 10) {
        menuStateStep(st, &(NONE = pressStart()));
    }
}

static void optionsTo(MenuState *st, int row)
{
    for (int guard = 0; guard <= MENU_OPTIONS_ROWS_FULL; ++guard) {
        if (menuStateOptionsRowAt(st, st->cursor) == row) {
            return;
        }
        menuStateStep(st, &(NONE = press("down")));
    }
    CHECK(!"options row never came under the cursor");
}

static void freshOptions(MenuState *st, int reached)
{
    memset(st, 0, sizeof(*st));
    st->capturing = -1;
    st->reached = reached;
    menuStateEnterOptions(st, MENU_TITLE);
}

static void openControls(MenuState *st)
{
    KeyMap defaults;
    keymapDefaults(&defaults);
    keymapSetActive(&defaults);

    memset(st, 0, sizeof(*st));
    st->hasContinue = true;
    menuStateEnterPause(st);

    for (int i = 0; i < 3; ++i) {
        menuStateStep(st, &(NONE = press("down")));
    }
    menuStateStep(st, &(NONE = press("confirm")));
}

static void freshPause(MenuState *st, bool hasContinue = true)
{
    memset(st, 0, sizeof(*st));
    st->hasContinue = hasContinue;
    menuStateEnterPause(st);
}

static void freshDeath(MenuState *st, bool hasContinue = true)
{
    memset(st, 0, sizeof(*st));
    st->hasContinue = hasContinue;
    menuStateEnterDeath(st);
}

static void rowsDown(MenuState *st, int n)
{
    for (int i = 0; i < n; ++i) {
        menuStateStep(st, &(NONE = press("down")));
    }
}

static void test_title_without_a_save_drops_the_continue_row(void)
{
    MenuState st;
    freshTitle(&st, false);
    CHECK_EQ(st.screen, MENU_TITLE);
    CHECK_EQ(st.cursor, 0);
    CHECK_EQ(menuStateTitleRowCount(&st), MENU_TITLE_ROWS_FULL - 1);

    CHECK_EQ(menuStateTitleRowAt(&st, 0), 0);
    CHECK_EQ(menuStateTitleRowAt(&st, 1), MENU_TITLE_ROW_OPTIONS);

    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))),
             MENU_ACT_START_GAME);
}

static void test_title_cursor_wraps_across_two_rows(void)
{
    MenuState st;
    freshTitle(&st, false);

    CHECK_EQ(menuStateStep(&st, &(NONE = press("down"))), MENU_ACT_NONE);
    CHECK_EQ(st.cursor, 1);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("down"))), MENU_ACT_NONE);
    CHECK_EQ(st.cursor, 0);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("up"))), MENU_ACT_NONE);
    CHECK_EQ(st.cursor, 1);
}

static void test_title_starts_on_continue_when_there_is_one(void)
{
    MenuState st;
    freshTitle(&st, true);

    CHECK_EQ(st.cursor, 1);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))),
             MENU_ACT_LOAD_GAME);
}

static void test_title_start_game_is_the_first_row(void)
{
    MenuState st;
    freshTitle(&st, true);

    CHECK_EQ(menuStateStep(&st, &(NONE = press("up"))), MENU_ACT_NONE);
    CHECK_EQ(st.cursor, 0);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))),
             MENU_ACT_START_GAME);
}

static void test_title_cursor_wraps_across_three_rows(void)
{
    MenuState st;
    freshTitle(&st, true);

    CHECK_EQ(menuStateTitleRowCount(&st), MENU_TITLE_ROWS_FULL);
    CHECK_EQ(st.cursor, MENU_TITLE_ROW_CONTINUE);
    menuStateStep(&st, &(NONE = press("down")));
    CHECK_EQ(st.cursor, MENU_TITLE_ROW_OPTIONS);
    menuStateStep(&st, &(NONE = press("down")));
    CHECK_EQ(st.cursor, 0);
    menuStateStep(&st, &(NONE = press("up")));
    CHECK_EQ(st.cursor, MENU_TITLE_ROW_OPTIONS);
}

static void test_a_level_select_cursor_does_not_survive_into_the_title(void)
{
    MenuState st;

    memset(&st, 0, sizeof(st));
    st.capturing = -1;
    menuStateEnterLevelSelect(&st, MENU_TITLE, true);
    st.cursor = 20;

    menuStateStep(&st, &(NONE = press("cancel")));
    CHECK_EQ(st.screen, MENU_TITLE);

    menuStateStep(&st, &(NONE = press("pause")));
    CHECK(st.cursor < menuStateTitleRowCount(&st));

    CHECK(menuStateStep(&st, &(NONE = press("confirm"))) != MENU_ACT_START_GAME);
}

static void test_title_continue_asks_nothing(void)
{
    MenuState st;
    freshTitle(&st, true);

    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))),
             MENU_ACT_LOAD_GAME);
    CHECK_EQ(st.screen, MENU_TITLE);
}

static void test_title_options_row_opens_the_options_screen(void)
{
    MenuState st;
    freshTitle(&st, true);

    rowsDown(&st, 1);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_OPTIONS);
    CHECK_EQ(st.cursor, 0);

    CHECK_EQ(menuStateStep(&st, &(NONE = press("cancel"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_TITLE);
}

static void test_title_options_row_moves_up_without_a_save(void)
{
    MenuState st;
    freshTitle(&st, false);

    rowsDown(&st, 1);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_OPTIONS);
}

static void test_konami_opens_the_level_select_with_everything()
{
    MenuState st;

    freshTitle(&st, false);
    st.reached = 0;
    enterKonami(&st, 11);

    CHECK_EQ(st.screen, MENU_LEVEL_SELECT);
    CHECK(st.cheatUnlocked);
    CHECK_EQ(menuStateLevelShown(&st), CHECKPOINT_COUNT);
    CHECK_EQ(st.returnScreen, MENU_TITLE);

    CHECK_EQ(st.cheatCount, 0);
}

static void test_start_finishes_the_code_and_the_a_starts_nothing()
{
    MenuState st;

    freshTitle(&st, false);
    enterKonami(&st, 9);

    CHECK_EQ(menuStateStep(&st, &(NONE = pressA())), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_TITLE);

    CHECK_EQ(menuStateStep(&st, &(NONE = pressStart())), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_LEVEL_SELECT);
}

static void test_c_finishes_the_code_the_way_a_does(void)
{
    MenuState st;

    freshTitle(&st, false);
    enterKonami(&st, 9);
    menuStateStep(&st, &(NONE = pressC()));

    CHECK_EQ(menuStateStep(&st, &(NONE = pressStart())), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_LEVEL_SELECT);
}

static void test_a_bare_confirm_still_starts_a_game()
{
    MenuState st;

    freshTitle(&st, false);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))),
             MENU_ACT_START_GAME);
}

static void test_a_wrong_press_restarts_the_sequence()
{
    MenuState st;

    freshTitle(&st, false);
    enterKonami(&st, 4);

    menuStateStep(&st, &(NONE = press("right")));
    menuStateStep(&st, &(NONE = press("right")));
    menuStateStep(&st, &(NONE = press("left")));
    menuStateStep(&st, &(NONE = press("right")));
    menuStateStep(&st, &(NONE = pressB()));
    menuStateStep(&st, &(NONE = pressA()));
    menuStateStep(&st, &(NONE = pressStart()));

    CHECK_EQ(st.screen, MENU_TITLE);
}

static void test_a_stray_press_does_not_stop_a_later_full_entry()
{
    MenuState st;

    freshTitle(&st, false);
    menuStateStep(&st, &(NONE = press("down")));
    enterKonami(&st, 11);
    CHECK_EQ(st.screen, MENU_LEVEL_SELECT);
}

static void test_a_stray_up_before_a_clean_entry_still_completes_it(void)
{
    MenuState st;

    freshTitle(&st, false);
    menuStateStep(&st, &(NONE = press("up")));
    enterKonami(&st, 11);
    CHECK_EQ(st.screen, MENU_LEVEL_SELECT);
}

static void test_auto_repeat_does_not_feed_the_cheat(void)
{
    static const char *const SEQ[8] = {
        "up", "up", "down", "down", "left", "right", "left", "right"
    };
    MenuState st;

    freshTitle(&st, false);

    for (int i = 0; i < 8; ++i) {
        menuStateStep(&st, &(NONE = press(SEQ[i])));
        menuStateStep(&st, &(NONE = repeated(SEQ[i])));
        menuStateStep(&st, &(NONE = repeated(SEQ[i])));
    }
    menuStateStep(&st, &(NONE = pressB()));
    menuStateStep(&st, &(NONE = pressA()));

    CHECK_EQ(menuStateStep(&st, &(NONE = pressStart())), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_LEVEL_SELECT);
}

static void test_a_missed_attempt_does_not_start_a_game(void)
{
    MenuState st;

    freshTitle(&st, false);
    enterKonami(&st, 4);
    menuStateStep(&st, &(NONE = press("right")));

    CHECK_EQ(menuStateStep(&st, &(NONE = pressB())), MENU_ACT_NONE);
    CHECK_EQ(menuStateStep(&st, &(NONE = pressA())), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_TITLE);

    CHECK_EQ(menuStateStep(&st, &(NONE = pressA())), MENU_ACT_START_GAME);
}

static void test_the_cursor_ends_where_the_code_started_it()
{
    MenuState st;

    freshTitle(&st, true);
    const int before = st.cursor;
    enterKonami(&st, 8);
    CHECK_EQ(st.cursor, before);
}

static void test_entering_the_title_clears_a_half_entered_code()
{
    MenuState st;

    freshTitle(&st, false);
    enterKonami(&st, 8);
    menuStateEnterTitle(&st);
    menuStateStep(&st, &(NONE = pressB()));
    menuStateStep(&st, &(NONE = pressA()));
    menuStateStep(&st, &(NONE = pressStart()));
    CHECK_EQ(st.screen, MENU_TITLE);
}

static void test_options_opens_on_its_first_row()
{
    MenuState st;
    freshOptions(&st, 5);
    CHECK_EQ(st.screen, MENU_OPTIONS);
    CHECK_EQ(st.cursor, 0);
    CHECK_EQ(menuStateOptionsRowAt(&st, 0), MENU_OPTIONS_ROW_LEVELS);
}

static void test_options_always_offers_the_level_row()
{
    MenuState st;
    freshOptions(&st, 0);
    CHECK_EQ(menuStateOptionsRowCount(&st), MENU_OPTIONS_ROWS_FULL);
    CHECK_EQ(menuStateOptionsRowAt(&st, 0), MENU_OPTIONS_ROW_LEVELS);
    CHECK_EQ(menuStateOptionsRowAt(&st, 1), MENU_OPTIONS_ROW_CONTROLS);
    CHECK_EQ(menuStateOptionsRowAt(&st, 2), MENU_OPTIONS_ROW_BACK);
}

static void test_options_keeps_the_level_row_with_progress()
{
    MenuState st;
    freshOptions(&st, 1);
    CHECK_EQ(menuStateOptionsRowCount(&st), MENU_OPTIONS_ROWS_FULL);
    CHECK_EQ(menuStateOptionsRowAt(&st, 1), MENU_OPTIONS_ROW_CONTROLS);
}

static void test_options_controls_row_opens_the_controls_screen()
{
    MenuState st;
    KeyMap defaults;

    keymapDefaults(&defaults);
    keymapSetActive(&defaults);

    freshOptions(&st, 0);
    optionsTo(&st, MENU_OPTIONS_ROW_CONTROLS);
    menuStateStep(&st, &(NONE = press("confirm")));
    CHECK_EQ(st.screen, MENU_CONTROLS);

    menuStateStep(&st, &(NONE = press("cancel")));
    CHECK_EQ(st.screen, MENU_OPTIONS);
}

static void test_options_back_row_and_cancel_both_return()
{
    MenuState st;

    freshOptions(&st, 0);
    optionsTo(&st, MENU_OPTIONS_ROW_BACK);
    menuStateStep(&st, &(NONE = press("confirm")));
    CHECK_EQ(st.screen, MENU_TITLE);

    freshOptions(&st, 0);
    menuStateStep(&st, &(NONE = press("cancel")));
    CHECK_EQ(st.screen, MENU_TITLE);
}

static void test_options_still_returns_to_the_title_after_a_controls_detour()
{
    MenuState st;
    KeyMap defaults;

    keymapDefaults(&defaults);
    keymapSetActive(&defaults);

    freshOptions(&st, 0);
    optionsTo(&st, MENU_OPTIONS_ROW_CONTROLS);
    menuStateStep(&st, &(NONE = press("confirm")));
    CHECK_EQ(st.screen, MENU_CONTROLS);

    menuStateStep(&st, &(NONE = press("cancel")));
    CHECK_EQ(st.screen, MENU_OPTIONS);

    menuStateStep(&st, &(NONE = press("cancel")));
    CHECK_EQ(st.screen, MENU_TITLE);
}

static void test_options_back_row_still_returns_to_the_title_after_a_controls_detour()
{
    MenuState st;
    KeyMap defaults;

    keymapDefaults(&defaults);
    keymapSetActive(&defaults);

    freshOptions(&st, 0);
    optionsTo(&st, MENU_OPTIONS_ROW_CONTROLS);
    menuStateStep(&st, &(NONE = press("confirm")));
    CHECK_EQ(st.screen, MENU_CONTROLS);

    menuStateStep(&st, &(NONE = press("cancel")));
    CHECK_EQ(st.screen, MENU_OPTIONS);

    optionsTo(&st, MENU_OPTIONS_ROW_BACK);
    menuStateStep(&st, &(NONE = press("confirm")));
    CHECK_EQ(st.screen, MENU_TITLE);
}

static void freshLevels(MenuState *st, int reached, bool all)
{
    memset(st, 0, sizeof(*st));
    st->capturing = -1;
    st->reached = reached;
    menuStateEnterLevelSelect(st, MENU_OPTIONS, all);
}

static void test_levels_show_a_prefix_of_the_table()
{
    MenuState st;

    freshLevels(&st, 5, false);
    CHECK_EQ(menuStateLevelShown(&st), 5);

    freshLevels(&st, 5, true);
    CHECK_EQ(menuStateLevelShown(&st), CHECKPOINT_COUNT);
}

static void test_a_zero_mark_still_offers_the_first_code()
{
    MenuState st;

    freshLevels(&st, 0, false);
    CHECK_EQ(menuStateLevelShown(&st), 1);
}

static void test_level_rows_break_at_chapter_boundaries()
{
    MenuState st;
    int idx = 0;

    freshLevels(&st, CHECKPOINT_COUNT, false);

    for (int row = 0; row < menuStateLevelRowCount(&st); ++row) {
        CHECK_EQ(menuStateLevelRowStart(&st, row), idx);
        CHECK(menuStateLevelRowLen(&st, row) >= 1);
        CHECK(menuStateLevelRowLen(&st, row) <= 3);

        const int chapter = checkpointChapterOf(idx);
        const int last = idx + menuStateLevelRowLen(&st, row) - 1;
        CHECK_EQ(checkpointChapterOf(last), chapter);

        idx += menuStateLevelRowLen(&st, row);
    }
    CHECK_EQ(idx, CHECKPOINT_COUNT);
}

static void test_left_and_right_walk_the_whole_list_and_wrap()
{
    MenuState st;

    freshLevels(&st, CHECKPOINT_COUNT, false);
    CHECK_EQ(st.cursor, 0);

    menuStateStep(&st, &(NONE = press("left")));
    CHECK_EQ(st.cursor, CHECKPOINT_COUNT - 1);

    menuStateStep(&st, &(NONE = press("right")));
    CHECK_EQ(st.cursor, 0);

    for (int i = 1; i < CHECKPOINT_COUNT; ++i) {
        menuStateStep(&st, &(NONE = press("right")));
        CHECK_EQ(st.cursor, i);
    }
}

static void test_up_and_down_clamp_on_short_rows_and_at_the_ends()
{
    MenuState st;

    freshLevels(&st, CHECKPOINT_COUNT, false);

    menuStateStep(&st, &(NONE = press("up")));
    CHECK_EQ(st.cursor, 0);

    for (int row = 1; row < menuStateLevelRowCount(&st); ++row) {
        st.cursor = menuStateLevelRowStart(&st, row - 1);
        menuStateStep(&st, &(NONE = press("down")));
        CHECK_EQ(menuStateLevelRowOf(&st, st.cursor), row);
        CHECK(st.cursor >= menuStateLevelRowStart(&st, row));
        CHECK(st.cursor <
              menuStateLevelRowStart(&st, row) + menuStateLevelRowLen(&st, row));
    }
}

static void test_confirm_jumps_to_the_cursor()
{
    MenuState st;

    freshLevels(&st, CHECKPOINT_COUNT, false);
    menuStateStep(&st, &(NONE = press("right")));
    menuStateStep(&st, &(NONE = press("right")));

    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))),
             MENU_ACT_LEVEL_JUMP);
    CHECK_EQ(st.levelChoice, 2);
}

static void test_cancel_returns_to_the_screen_that_opened_it()
{
    MenuState st;

    freshLevels(&st, 3, false);
    menuStateStep(&st, &(NONE = press("cancel")));
    CHECK_EQ(st.screen, MENU_OPTIONS);

    memset(&st, 0, sizeof(st));
    st.capturing = -1;
    st.reached = 3;
    menuStateEnterLevelSelect(&st, MENU_TITLE, true);
    menuStateStep(&st, &(NONE = press("cancel")));
    CHECK_EQ(st.screen, MENU_TITLE);
}

static void test_the_unlock_outlasts_the_screen_that_opened_it()
{
    MenuState st;

    memset(&st, 0, sizeof(st));
    st.capturing = -1;
    st.reached = 3;

    menuStateEnterLevelSelect(&st, MENU_TITLE, true);
    CHECK_EQ(menuStateLevelShown(&st), CHECKPOINT_COUNT);

    menuStateStep(&st, &(NONE = press("cancel")));
    CHECK_EQ(st.screen, MENU_TITLE);
    CHECK(st.cheatUnlocked);

    menuStateEnterLevelSelect(&st, MENU_OPTIONS, false);
    CHECK(st.cheatUnlocked);
    CHECK_EQ(menuStateLevelShown(&st), CHECKPOINT_COUNT);
}

static void test_vertical_movement_clamps_a_wide_column_onto_a_short_row()
{
    MenuState st;
    int rows;
    int downRow = -1;
    int upRow = -1;

    freshLevels(&st, CHECKPOINT_COUNT, false);
    rows = menuStateLevelRowCount(&st);

    for (int row = 0; row + 1 < rows; ++row) {
        if (menuStateLevelRowLen(&st, row) == 3 &&
            menuStateLevelRowLen(&st, row + 1) < 3) {
            downRow = row;
            break;
        }
    }

    CHECK(downRow >= 0);
    if (downRow >= 0) {
        st.cursor = menuStateLevelRowStart(&st, downRow) +
                    menuStateLevelRowLen(&st, downRow) - 1;
        menuStateStep(&st, &(NONE = press("down")));
        CHECK_EQ(st.cursor,
                 menuStateLevelRowStart(&st, downRow + 1) +
                     menuStateLevelRowLen(&st, downRow + 1) - 1);
    }

    for (int row = 1; row < rows; ++row) {
        if (menuStateLevelRowLen(&st, row) == 3 &&
            menuStateLevelRowLen(&st, row - 1) < 3) {
            upRow = row;
            break;
        }
    }

    CHECK(upRow >= 0);
    if (upRow >= 0) {
        freshLevels(&st, CHECKPOINT_COUNT, false);
        st.cursor = menuStateLevelRowStart(&st, upRow) +
                    menuStateLevelRowLen(&st, upRow) - 1;
        menuStateStep(&st, &(NONE = press("up")));
        CHECK_EQ(st.cursor,
                 menuStateLevelRowStart(&st, upRow - 1) +
                     menuStateLevelRowLen(&st, upRow - 1) - 1);
    }
}

static void test_partly_reached_grid_at_five()
{
    MenuState st;

    freshLevels(&st, 5, false);

    CHECK_EQ(menuStateLevelRowCount(&st), 3);

    CHECK_EQ(menuStateLevelRowStart(&st, 0), 0);
    CHECK_EQ(menuStateLevelRowLen(&st, 0), 1);
    CHECK_EQ(menuStateLevelRowStart(&st, 1), 1);
    CHECK_EQ(menuStateLevelRowLen(&st, 1), 1);
    CHECK_EQ(menuStateLevelRowStart(&st, 2), 2);
    CHECK_EQ(menuStateLevelRowLen(&st, 2), 3);

    CHECK_EQ(menuStateLevelRowLen(&st, 0) + menuStateLevelRowLen(&st, 1) +
                 menuStateLevelRowLen(&st, 2),
             5);

    CHECK_EQ(menuStateLevelRowOf(&st, 0), 0);
    CHECK_EQ(menuStateLevelRowOf(&st, 1), 1);
    CHECK_EQ(menuStateLevelRowOf(&st, 2), 2);
    CHECK_EQ(menuStateLevelRowOf(&st, 3), 2);
    CHECK_EQ(menuStateLevelRowOf(&st, 4), 2);

    CHECK_EQ(menuStateLevelRowOf(&st, 5), -1);
}

static void test_partly_reached_grid_completes_a_chapter_exactly()
{
    MenuState st;
    int shown;
    int sum = 0;

    freshLevels(&st, 11, false);
    shown = menuStateLevelShown(&st);
    CHECK_EQ(shown, 11);

    CHECK_EQ(menuStateLevelRowCount(&st), 5);

    for (int row = 0; row < menuStateLevelRowCount(&st); ++row) {
        sum += menuStateLevelRowLen(&st, row);
    }
    CHECK_EQ(sum, shown);
}

static void test_an_unlocked_session_hides_the_pause_save_row(void)
{
    MenuState st;

    freshPause(&st);
    st.cheatUnlocked = true;

    CHECK_EQ(menuStateRowCount(&st), MENU_ROWS_FULL - 1);
    CHECK_EQ(menuStateRowAt(&st, 0), 0);
    CHECK_EQ(menuStateRowAt(&st, 1), MENU_ROW_LOAD);
    CHECK_EQ(menuStateRowAt(&st, 2), MENU_ROW_DEATH_SAVE_QUIT);
    CHECK_EQ(menuStateRowAt(&st, 3), MENU_ROWS_FULL - 1);

    for (int cursor = 0; cursor < menuStateRowCount(&st); ++cursor) {
        CHECK(menuStateRowAt(&st, cursor) != MENU_ROW_SAVE);
    }
}

static void test_an_unlocked_session_hides_both_death_save_rows(void)
{
    MenuState st;

    freshDeath(&st);
    st.cheatUnlocked = true;

    CHECK_EQ(menuStateRowCount(&st), 3);
    CHECK_EQ(menuStateRowAt(&st, 0), 0);
    CHECK_EQ(menuStateRowAt(&st, 1), MENU_ROW_LOAD);
    CHECK_EQ(menuStateRowAt(&st, 2), MENU_ROWS_FULL - 1);

    freshDeath(&st, false);
    st.cheatUnlocked = true;

    CHECK_EQ(menuStateRowCount(&st), 3);
    CHECK_EQ(menuStateRowAt(&st, 0), 0);
    CHECK_EQ(menuStateRowAt(&st, 1), MENU_ROW_LOAD);
    CHECK_EQ(menuStateRowAt(&st, 2), MENU_ROWS_FULL - 1);
}

static void test_the_cheat_swaps_the_death_load_row_for_the_level_select(void)
{
    MenuState st;

    freshDeath(&st);
    st.cheatUnlocked = true;
    CHECK_EQ(menuStateRowAt(&st, 1), MENU_ROW_LOAD);

    menuStateStep(&st, &(NONE = press("down")));
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_LEVEL_SELECT);

    freshDeath(&st, false);
    st.cheatUnlocked = true;
    CHECK_EQ(menuStateRowAt(&st, 1), MENU_ROW_LOAD);
    menuStateStep(&st, &(NONE = press("down")));
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_LEVEL_SELECT);
}

static void test_the_cheat_swaps_the_pause_load_row_for_the_level_select(void)
{
    MenuState st;

    freshPause(&st);
    st.cheatUnlocked = true;
    CHECK_EQ(menuStateRowAt(&st, 1), MENU_ROW_LOAD);

    menuStateStep(&st, &(NONE = press("down")));
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_LEVEL_SELECT);

    freshPause(&st, false);
    st.cheatUnlocked = true;
    CHECK_EQ(menuStateRowCount(&st), MENU_ROWS_FULL - 1);
    CHECK_EQ(menuStateRowAt(&st, 1), MENU_ROW_LOAD);
    menuStateStep(&st, &(NONE = press("down")));
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_LEVEL_SELECT);

    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))), MENU_ACT_LEVEL_JUMP);
    CHECK_EQ(st.screen, MENU_PAUSE);
}

static void test_an_unlocked_session_can_reach_no_save_action(void)
{
    static const MenuScreen SCREENS[2] = { MENU_PAUSE, MENU_DEATH };

    for (int s = 0; s < 2; ++s) {
        for (int hasSave = 0; hasSave < 2; ++hasSave) {
            MenuState st;

            if (SCREENS[s] == MENU_PAUSE) {
                freshPause(&st, hasSave != 0);
            } else {
                freshDeath(&st, hasSave != 0);
            }
            st.cheatUnlocked = true;

            for (int cursor = 0; cursor < menuStateRowCount(&st); ++cursor) {
                MenuState probe = st;
                MenuAction act;

                probe.cursor = cursor;
                act = menuStateStep(&probe, &(NONE = press("confirm")));

                CHECK(act != MENU_ACT_SAVE_GAME);
                CHECK(act != MENU_ACT_SAVE_RETRY);
                CHECK(act != MENU_ACT_SAVE_AND_QUIT);
            }
        }
    }
}

static void test_pause_starts_on_resume(void)
{
    MenuState st;
    freshPause(&st);
    CHECK_EQ(st.screen, MENU_PAUSE);
    CHECK_EQ(st.cursor, 0);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))), MENU_ACT_RESUME);
}

static void test_pause_button_resumes(void)
{
    MenuState st;
    freshPause(&st);
    menuStateStep(&st, &(NONE = press("down")));
    CHECK_EQ(menuStateStep(&st, &(NONE = press("pause"))), MENU_ACT_RESUME);
}

static void test_pause_cancel_resumes(void)
{
    MenuState st;
    freshPause(&st);
    menuStateStep(&st, &(NONE = press("down")));
    CHECK_EQ(menuStateStep(&st, &(NONE = press("cancel"))), MENU_ACT_RESUME);
}

static void test_pause_cursor_wraps_across_five_rows(void)
{
    MenuState st;
    freshPause(&st);
    CHECK_EQ(menuStateRowCount(&st), 5);

    menuStateStep(&st, &(NONE = press("up")));
    CHECK_EQ(st.cursor, 4);
    menuStateStep(&st, &(NONE = press("down")));
    CHECK_EQ(st.cursor, 0);

    for (int i = 0; i < 5; ++i) {
        CHECK_EQ(st.cursor, i);
        menuStateStep(&st, &(NONE = press("down")));
    }
    CHECK_EQ(st.cursor, 0);
}

static void test_pause_without_a_save_drops_the_load_row(void)
{
    MenuState st;
    freshPause(&st, false);

    CHECK_EQ(menuStateRowCount(&st), 4);

    CHECK_EQ(menuStateRowAt(&st, 0), 0);
    CHECK_EQ(menuStateRowAt(&st, 1), 1);
    CHECK_EQ(menuStateRowAt(&st, 2), 3);
    CHECK_EQ(menuStateRowAt(&st, 3), 4);

    rowsDown(&st, 2);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_CONTROLS);
}

static void test_pause_without_a_save_wraps_across_four_rows(void)
{
    MenuState st;
    freshPause(&st, false);

    menuStateStep(&st, &(NONE = press("up")));
    CHECK_EQ(st.cursor, 3);
    menuStateStep(&st, &(NONE = press("down")));
    CHECK_EQ(st.cursor, 0);

    rowsDown(&st, 3);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_CONFIRM);
    CHECK_EQ(st.pending, MENU_ACT_RETURN_TO_TITLE);
}

static void test_a_vanishing_save_brings_the_cursor_back_inside(void)
{
    MenuState st;
    freshPause(&st);

    rowsDown(&st, 4);
    CHECK_EQ(st.cursor, 4);
    st.hasContinue = false;

    CHECK_EQ(menuStateStep(&st, &(NONE = press("none"))), MENU_ACT_NONE);
    CHECK_EQ(st.cursor, 3);
    CHECK_EQ(menuStateRowAt(&st, st.cursor), 4);
}

static void test_pause_save_asks_only_with_a_save_to_overwrite(void)
{
    MenuState st;

    freshPause(&st);
    st.hasContinue = false;
    menuStateStep(&st, &(NONE = press("down")));
    CHECK_EQ(st.cursor, 1);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))),
             MENU_ACT_SAVE_GAME);
    CHECK_EQ(st.screen, MENU_PAUSE);

    freshPause(&st);
    st.hasContinue = true;
    menuStateStep(&st, &(NONE = press("down")));
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_CONFIRM);
    CHECK_EQ(st.confirmYes, true);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))),
             MENU_ACT_SAVE_GAME);
    CHECK_EQ(st.screen, MENU_PAUSE);

    freshPause(&st);
    st.hasContinue = true;
    menuStateStep(&st, &(NONE = press("down")));
    menuStateStep(&st, &(NONE = press("confirm")));
    menuStateStep(&st, &(NONE = press("right")));
    CHECK_EQ(st.confirmYes, false);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_PAUSE);
}

static void test_pause_saved_notice_dismisses_back(void)
{
    MenuState st;
    freshPause(&st);

    menuStateShowSaved(&st);
    CHECK_EQ(st.screen, MENU_NOTICE);

    const char *keys[3];
    keys[0] = "confirm";
    keys[1] = "cancel";
    keys[2] = "pause";
    for (int i = 0; i < 3; ++i) {
        MenuState probe = st;
        CHECK_EQ(menuStateStep(&probe, &(NONE = press(keys[i]))),
                 MENU_ACT_NONE);
        CHECK_EQ(probe.screen, MENU_PAUSE);
    }

    CHECK_EQ(menuStateStep(&st, &(NONE = press("down"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_NOTICE);
}

static void test_pause_load_asks_first(void)
{
    MenuState st;
    freshPause(&st);
    menuStateStep(&st, &(NONE = press("down")));
    menuStateStep(&st, &(NONE = press("down")));
    CHECK_EQ(st.cursor, 2);

    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_CONFIRM);
    CHECK_EQ(st.pending, MENU_ACT_LOAD_GAME);

    CHECK(st.confirmYes);
}

static void test_pause_controls_row_moves_up_without_a_save(void)
{
    MenuState st;

    freshPause(&st);
    rowsDown(&st, 3);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_CONTROLS);

    freshPause(&st, false);
    rowsDown(&st, 3);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_CONFIRM);
    CHECK_EQ(st.pending, MENU_ACT_RETURN_TO_TITLE);
}

static void test_pause_controls_row_opens_the_screen(void)
{
    MenuState st;
    openControls(&st);

    CHECK_EQ(st.screen, MENU_CONTROLS);
    CHECK_EQ(st.cursor, 0);
    CHECK_EQ(st.capturing, -1);
    CHECK(!st.mapDirty);
    CHECK(keymapValid(&st.map));
}

static void test_controls_cursor_wraps_across_every_row(void)
{
    MenuState st;
    openControls(&st);

    menuStateStep(&st, &(NONE = press("up")));
    CHECK_EQ(st.cursor, MENU_CONTROLS_ROWS - 1);
    menuStateStep(&st, &(NONE = press("down")));
    CHECK_EQ(st.cursor, 0);

    for (int i = 0; i < MENU_CONTROLS_ROWS; ++i) {
        CHECK_EQ(st.cursor, i);
        menuStateStep(&st, &(NONE = press("down")));
    }
    CHECK_EQ(st.cursor, 0);
}

static void test_confirm_on_a_binding_row_starts_a_capture(void)
{
    MenuState st;
    openControls(&st);

    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))), MENU_ACT_NONE);
    CHECK_EQ(st.capturing, KEYMAP_ROW_ACTION);
    CHECK(!st.mapDirty);
}

static void test_a_capture_binds_and_swaps(void)
{
    MenuState st;
    openControls(&st);

    menuStateStep(&st, &(NONE = press("confirm")));
    CHECK_EQ(menuStateStep(&st, &(NONE = capture(PAD_C))), MENU_ACT_NONE);

    CHECK_EQ(st.capturing, -1);
    CHECK(st.mapDirty);
    CHECK_EQ(st.map.row[KEYMAP_ROW_ACTION], PAD_C);
    CHECK_EQ(st.map.row[KEYMAP_ROW_JUMP], PAD_A);
}

static void test_a_capture_of_the_button_already_there_changes_nothing(void)
{
    MenuState st;
    openControls(&st);

    menuStateStep(&st, &(NONE = press("confirm")));
    menuStateStep(&st, &(NONE = capture(PAD_A)));

    CHECK_EQ(st.capturing, -1);
    CHECK(!st.mapDirty);
    CHECK_EQ(st.map.row[KEYMAP_ROW_ACTION], PAD_A);
}

static void test_start_aborts_a_capture(void)
{
    MenuState st;
    openControls(&st);

    menuStateStep(&st, &(NONE = press("confirm")));
    CHECK_EQ(st.capturing, KEYMAP_ROW_ACTION);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("pause"))), MENU_ACT_NONE);
    CHECK_EQ(st.capturing, -1);
    CHECK(!st.mapDirty);
    CHECK_EQ(st.screen, MENU_CONTROLS);
}

static void test_a_capture_swallows_cancel_and_the_cursor(void)
{
    MenuState st;
    openControls(&st);

    menuStateStep(&st, &(NONE = press("confirm")));

    CHECK_EQ(menuStateStep(&st, &(NONE = press("cancel"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_CONTROLS);
    CHECK_EQ(st.capturing, KEYMAP_ROW_ACTION);

    menuStateStep(&st, &(NONE = press("down")));
    CHECK_EQ(st.cursor, 0);
    CHECK_EQ(st.capturing, KEYMAP_ROW_ACTION);
}

static void test_reset_restores_the_defaults(void)
{
    MenuState st;
    KeyMap defaults;
    openControls(&st);
    keymapDefaults(&defaults);

    menuStateStep(&st, &(NONE = press("confirm")));
    menuStateStep(&st, &(NONE = capture(PAD_Z)));
    CHECK_EQ(st.map.row[KEYMAP_ROW_ACTION], PAD_Z);

    for (int i = 0; i < MENU_CONTROLS_ROW_RESET; ++i) {
        menuStateStep(&st, &(NONE = press("down")));
    }
    CHECK_EQ(st.cursor, MENU_CONTROLS_ROW_RESET);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))), MENU_ACT_NONE);

    for (int i = 0; i < KEYMAP_ROW_COUNT; ++i) {
        CHECK_EQ(st.map.row[i], defaults.row[i]);
    }
    CHECK(st.mapDirty);
    CHECK_EQ(st.screen, MENU_CONTROLS);
}

static void test_reset_on_an_untouched_map_is_not_dirty(void)
{
    MenuState st;
    openControls(&st);

    for (int i = 0; i < MENU_CONTROLS_ROW_RESET; ++i) {
        menuStateStep(&st, &(NONE = press("down")));
    }
    menuStateStep(&st, &(NONE = press("confirm")));
    CHECK(!st.mapDirty);
}

static void test_leaving_unchanged_asks_for_no_save(void)
{
    MenuState st;

    openControls(&st);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("cancel"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_PAUSE);

    openControls(&st);
    for (int i = 0; i < MENU_CONTROLS_ROW_BACK; ++i) {
        menuStateStep(&st, &(NONE = press("down")));
    }
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_PAUSE);
}

static void test_leaving_changed_asks_for_a_save_once(void)
{
    MenuState st;
    openControls(&st);

    menuStateStep(&st, &(NONE = press("confirm")));
    menuStateStep(&st, &(NONE = capture(PAD_Y)));
    CHECK(st.mapDirty);

    CHECK_EQ(menuStateStep(&st, &(NONE = press("cancel"))),
             MENU_ACT_SAVE_KEYMAP);
    CHECK_EQ(st.screen, MENU_PAUSE);
    CHECK(!st.mapDirty);

    for (int i = 0; i < 3; ++i) {
        menuStateStep(&st, &(NONE = press("down")));
    }
    menuStateStep(&st, &(NONE = press("confirm")));
    CHECK_EQ(st.screen, MENU_CONTROLS);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("cancel"))), MENU_ACT_NONE);
}

static void test_the_screen_edits_a_copy(void)
{
    MenuState st;
    openControls(&st);

    menuStateStep(&st, &(NONE = press("confirm")));
    menuStateStep(&st, &(NONE = capture(PAD_X)));
    CHECK_EQ(st.map.row[KEYMAP_ROW_ACTION], PAD_X);

    CHECK_EQ(keymapActive()->row[KEYMAP_ROW_ACTION], PAD_A);
}

static void test_death_starts_on_load(void)
{
    MenuState st;
    freshDeath(&st);
    CHECK_EQ(st.screen, MENU_DEATH);
    CHECK_EQ(st.cursor, 0);
    CHECK_EQ(menuStateRowAt(&st, 0), MENU_ROW_LOAD);

    freshDeath(&st, false);
    CHECK_EQ(st.cursor, 0);
    CHECK_EQ(menuStateRowAt(&st, 0), 0);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))), MENU_ACT_RETRY);
}

static void test_death_load_asks_first(void)
{
    MenuState st;
    freshDeath(&st);

    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_CONFIRM);
    CHECK_EQ(st.pending, MENU_ACT_LOAD_GAME);
    CHECK(st.confirmYes);
}

static void test_death_resume_row(void)
{
    MenuState st;
    freshDeath(&st);
    menuStateStep(&st, &(NONE = press("down")));
    CHECK_EQ(st.cursor, 1);
    CHECK_EQ(menuStateRowAt(&st, 1), 0);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))), MENU_ACT_RETRY);
}

static void test_death_save_and_resume_row(void)
{
    MenuState st;
    freshDeath(&st);
    menuStateStep(&st, &(NONE = press("down")));
    menuStateStep(&st, &(NONE = press("down")));
    CHECK_EQ(st.cursor, 2);

    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))),
             MENU_ACT_SAVE_RETRY);
}

static void test_the_pause_menu_still_meets_save_before_load(void)
{
    MenuState st;
    freshPause(&st);

    CHECK_EQ(menuStateRowAt(&st, 1), MENU_ROW_SAVE);
    CHECK_EQ(menuStateRowAt(&st, 2), MENU_ROW_LOAD);
}

static void test_death_without_a_save_drops_the_load_row(void)
{
    MenuState st;
    freshDeath(&st, false);

    CHECK_EQ(menuStateRowCount(&st), 4);

    rowsDown(&st, 2);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))),
             MENU_ACT_SAVE_AND_QUIT);

    freshDeath(&st, false);
    rowsDown(&st, 3);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))),
             MENU_ACT_RETURN_TO_TITLE);
}

static void test_death_save_and_quit_row(void)
{
    MenuState st;
    freshDeath(&st);
    for (int i = 0; i < 3; ++i) {
        menuStateStep(&st, &(NONE = press("down")));
    }
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))),
             MENU_ACT_SAVE_AND_QUIT);
}

static void test_death_quit_row(void)
{
    MenuState st;
    freshDeath(&st);
    for (int i = 0; i < 4; ++i) {
        menuStateStep(&st, &(NONE = press("down")));
    }
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))),
             MENU_ACT_RETURN_TO_TITLE);
}

static void test_death_cancel_resumes_from_any_row(void)
{
    for (int row = 0; row < 5; ++row) {
        MenuState st;
        freshDeath(&st);
        for (int i = 0; i < row; ++i) {
            menuStateStep(&st, &(NONE = press("down")));
        }
        CHECK_EQ(menuStateStep(&st, &(NONE = press("cancel"))),
                 MENU_ACT_RETRY);
    }
}

static void test_death_cursor_wraps_across_five_rows(void)
{
    MenuState st;
    freshDeath(&st);
    CHECK_EQ(menuStateRowCount(&st), 5);
    menuStateStep(&st, &(NONE = press("up")));
    CHECK_EQ(st.cursor, 4);
    menuStateStep(&st, &(NONE = press("down")));
    CHECK_EQ(st.cursor, 0);

    freshDeath(&st, false);
    menuStateStep(&st, &(NONE = press("up")));
    CHECK_EQ(st.cursor, 3);
    menuStateStep(&st, &(NONE = press("down")));
    CHECK_EQ(st.cursor, 0);
}

static void test_return_to_menu_asks_for_confirmation(void)
{
    MenuState st;
    freshPause(&st);
    for (int i = 0; i < 4; ++i) {
        menuStateStep(&st, &(NONE = press("down")));
    }
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_CONFIRM);
    CHECK_EQ(st.pending, MENU_ACT_RETURN_TO_TITLE);
}

static void test_confirm_defaults_to_no(void)
{
    MenuState st;
    freshPause(&st);
    for (int i = 0; i < 4; ++i) {
        menuStateStep(&st, &(NONE = press("down")));
    }
    menuStateStep(&st, &(NONE = press("confirm")));
    CHECK(!st.confirmYes);

    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_PAUSE);
}

static void test_confirm_yes_returns_to_title(void)
{
    MenuState st;
    freshPause(&st);
    for (int i = 0; i < 4; ++i) {
        menuStateStep(&st, &(NONE = press("down")));
    }
    menuStateStep(&st, &(NONE = press("confirm")));
    menuStateStep(&st, &(NONE = press("right")));
    CHECK(st.confirmYes);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))),
             MENU_ACT_RETURN_TO_TITLE);
    CHECK_EQ(st.screen, MENU_TITLE);
}

static void test_confirm_cancel_declines(void)
{
    MenuState st;
    freshPause(&st);
    for (int i = 0; i < 4; ++i) {
        menuStateStep(&st, &(NONE = press("down")));
    }
    menuStateStep(&st, &(NONE = press("confirm")));
    menuStateStep(&st, &(NONE = press("right")));
    CHECK_EQ(menuStateStep(&st, &(NONE = press("cancel"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_PAUSE);
}

static void test_confirmed_load_returns_to_the_asking_screen(void)
{
    MenuState st;

    freshPause(&st);
    menuStateStep(&st, &(NONE = press("down")));
    menuStateStep(&st, &(NONE = press("down")));
    menuStateStep(&st, &(NONE = press("confirm")));
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))),
             MENU_ACT_LOAD_GAME);
    CHECK_EQ(st.screen, MENU_PAUSE);

    freshDeath(&st);
    menuStateStep(&st, &(NONE = press("confirm")));
    CHECK_EQ(menuStateStep(&st, &(NONE = press("confirm"))),
             MENU_ACT_LOAD_GAME);
    CHECK_EQ(st.screen, MENU_DEATH);
}

static void test_declined_load_returns_to_the_asking_screen(void)
{
    MenuState st;

    freshDeath(&st);
    menuStateStep(&st, &(NONE = press("confirm")));
    CHECK_EQ(menuStateStep(&st, &(NONE = press("cancel"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_DEATH);
}

static void test_confirm_flips_on_left_and_right(void)
{
    MenuState st;
    freshDeath(&st);
    menuStateStep(&st, &(NONE = press("confirm")));

    CHECK(st.confirmYes);
    menuStateStep(&st, &(NONE = press("left")));
    CHECK(!st.confirmYes);
    menuStateStep(&st, &(NONE = press("right")));
    CHECK(st.confirmYes);
}

static void test_reentering_a_screen_resets_the_prompt(void)
{
    MenuState st;
    freshDeath(&st);
    menuStateStep(&st, &(NONE = press("confirm")));
    CHECK(st.confirmYes);

    menuStateEnterDeath(&st);
    CHECK(!st.confirmYes);
    CHECK_EQ(st.pending, MENU_ACT_NONE);
}

static void test_empty_input_does_nothing(void)
{
    MenuState st;

    freshTitle(&st, false);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("none"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_TITLE);

    freshPause(&st);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("none"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_PAUSE);

    freshDeath(&st);
    CHECK_EQ(menuStateStep(&st, &(NONE = press("none"))), MENU_ACT_NONE);
    CHECK_EQ(st.screen, MENU_DEATH);
}

int main(void)
{
    test_title_without_a_save_drops_the_continue_row();
    test_title_cursor_wraps_across_two_rows();
    test_title_starts_on_continue_when_there_is_one();
    test_title_start_game_is_the_first_row();
    test_title_cursor_wraps_across_three_rows();
    test_title_continue_asks_nothing();
    test_title_options_row_opens_the_options_screen();
    test_title_options_row_moves_up_without_a_save();
    test_a_level_select_cursor_does_not_survive_into_the_title();

    test_konami_opens_the_level_select_with_everything();
    test_start_finishes_the_code_and_the_a_starts_nothing();
    test_c_finishes_the_code_the_way_a_does();
    test_auto_repeat_does_not_feed_the_cheat();
    test_a_missed_attempt_does_not_start_a_game();
    test_a_bare_confirm_still_starts_a_game();
    test_a_wrong_press_restarts_the_sequence();
    test_a_stray_press_does_not_stop_a_later_full_entry();
    test_a_stray_up_before_a_clean_entry_still_completes_it();
    test_the_cursor_ends_where_the_code_started_it();
    test_entering_the_title_clears_a_half_entered_code();

    test_options_opens_on_its_first_row();
    test_options_always_offers_the_level_row();
    test_a_zero_mark_still_offers_the_first_code();
    test_options_keeps_the_level_row_with_progress();
    test_options_controls_row_opens_the_controls_screen();
    test_options_back_row_and_cancel_both_return();
    test_options_still_returns_to_the_title_after_a_controls_detour();
    test_options_back_row_still_returns_to_the_title_after_a_controls_detour();

    test_levels_show_a_prefix_of_the_table();
    test_level_rows_break_at_chapter_boundaries();
    test_left_and_right_walk_the_whole_list_and_wrap();
    test_up_and_down_clamp_on_short_rows_and_at_the_ends();
    test_confirm_jumps_to_the_cursor();
    test_cancel_returns_to_the_screen_that_opened_it();
    test_the_unlock_outlasts_the_screen_that_opened_it();
    test_vertical_movement_clamps_a_wide_column_onto_a_short_row();
    test_partly_reached_grid_at_five();
    test_partly_reached_grid_completes_a_chapter_exactly();

    test_an_unlocked_session_hides_the_pause_save_row();
    test_an_unlocked_session_hides_both_death_save_rows();
    test_the_cheat_swaps_the_death_load_row_for_the_level_select();
    test_the_cheat_swaps_the_pause_load_row_for_the_level_select();
    test_an_unlocked_session_can_reach_no_save_action();
    test_pause_starts_on_resume();
    test_pause_button_resumes();
    test_pause_cancel_resumes();
    test_pause_cursor_wraps_across_five_rows();
    test_pause_save_asks_only_with_a_save_to_overwrite();
    test_pause_saved_notice_dismisses_back();
    test_pause_load_asks_first();
    test_pause_without_a_save_drops_the_load_row();
    test_pause_without_a_save_wraps_across_four_rows();
    test_a_vanishing_save_brings_the_cursor_back_inside();
    test_pause_controls_row_moves_up_without_a_save();
    test_pause_controls_row_opens_the_screen();
    test_controls_cursor_wraps_across_every_row();
    test_confirm_on_a_binding_row_starts_a_capture();
    test_a_capture_binds_and_swaps();
    test_a_capture_of_the_button_already_there_changes_nothing();
    test_start_aborts_a_capture();
    test_a_capture_swallows_cancel_and_the_cursor();
    test_reset_restores_the_defaults();
    test_reset_on_an_untouched_map_is_not_dirty();
    test_leaving_unchanged_asks_for_no_save();
    test_leaving_changed_asks_for_a_save_once();
    test_the_screen_edits_a_copy();

    test_death_starts_on_load();
    test_death_resume_row();
    test_death_load_asks_first();
    test_death_save_and_resume_row();
    test_the_pause_menu_still_meets_save_before_load();
    test_death_without_a_save_drops_the_load_row();
    test_death_save_and_quit_row();
    test_death_quit_row();
    test_death_cancel_resumes_from_any_row();
    test_death_cursor_wraps_across_five_rows();

    test_return_to_menu_asks_for_confirmation();
    test_confirm_defaults_to_no();
    test_confirm_yes_returns_to_title();
    test_confirm_cancel_declines();
    test_confirmed_load_returns_to_the_asking_screen();
    test_declined_load_returns_to_the_asking_screen();
    test_confirm_flips_on_left_and_right();
    test_reentering_a_screen_resets_the_prompt();
    test_empty_input_does_nothing();

    if (g_fail == 0) {
        printf("all tests passed\n");
        return 0;
    }
    printf("%d check(s) failed\n", g_fail);
    return 1;
}
