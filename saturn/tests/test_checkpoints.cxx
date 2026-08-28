#include <cstdio>
#include <cstring>
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

static void test_every_string_id_in_range_and_used_once()
{
    const int span = CHECKPOINT_STRING_LAST - CHECKPOINT_STRING_FIRST + 1;
    int seen[CHECKPOINT_STRING_LAST - CHECKPOINT_STRING_FIRST + 1];

    CHECK_EQ(CHECKPOINT_COUNT, 15);
    CHECK_EQ(span, 23);
    for (int k = 0; k < span; ++k) {
        seen[k] = 0;
    }
    for (int i = 0; i < CHECKPOINT_COUNT; ++i) {
        const int id = (int)checkpointStringId(i);
        CHECK(id >= CHECKPOINT_STRING_FIRST);
        CHECK(id <= CHECKPOINT_STRING_LAST);
        seen[id - CHECKPOINT_STRING_FIRST]++;
    }
    for (int k = 0; k < span; ++k) {
        CHECK(seen[k] == 0 || seen[k] == 1);
    }
}

static void test_the_unreachable_words_are_absent()
{
    static const unsigned short DEAD[8] = {
        0x167, 0x168, 0x16C, 0x16E, 0x170, 0x169, 0x171, 0x174
    };

    for (int k = 0; k < 8; ++k) {
        CHECK_EQ(checkpointOfStringId(DEAD[k]), -1);
    }
    CHECK_EQ(checkpointOfOrdinal(42), -1);
    CHECK_EQ(checkpointOfOrdinal(49), -1);
    CHECK_EQ(checkpointOfOrdinal(62), -1);
    CHECK_EQ(checkpointOfOrdinal(68), -1);
}

static void test_string_ids_map_back_to_their_checkpoints()
{
    for (int i = 0; i < CHECKPOINT_COUNT; ++i) {
        CHECK_EQ(checkpointOfStringId(checkpointStringId(i)), i);
    }
    CHECK_EQ(checkpointOfStringId(0), -1);
    CHECK_EQ(checkpointOfStringId(0x15D), -1);
    CHECK_EQ(checkpointOfStringId(0x175), -1);
}

static void test_every_word_is_four_letters()
{
    for (int i = 0; i < CHECKPOINT_COUNT; ++i) {
        CHECK_EQ(strlen(checkpointWord(i)), 4);
    }
}

static void test_chapters_partition_the_table_in_order()
{
    int next = 0;
    CHECK(checkpointChapterCount() > 0);
    for (int c = 0; c < checkpointChapterCount(); ++c) {
        CHECK_EQ(checkpointChapterFirst(c), next);
        CHECK(checkpointChapterLen(c) > 0);
        next += checkpointChapterLen(c);
    }
    CHECK_EQ(next, CHECKPOINT_COUNT);
}

static void test_chapter_parts_are_reachable_and_ascending()
{
    for (int c = 0; c < checkpointChapterCount(); ++c) {
        CHECK(checkpointChapterPart(c) >= 0x3E81);
        CHECK(checkpointChapterPart(c) <= 0x3E89);
        if (c > 0) {
            CHECK(checkpointChapterPart(c) > checkpointChapterPart(c - 1));
        }
    }
}

static void test_chapter_of_agrees_with_the_ranges()
{
    for (int c = 0; c < checkpointChapterCount(); ++c) {
        const int first = checkpointChapterFirst(c);
        for (int k = 0; k < checkpointChapterLen(c); ++k) {
            CHECK_EQ(checkpointChapterOf(first + k), c);
        }
    }
}

static void test_the_sorted_table_matches_the_probe()
{
    CHECK_EQ(checkpointChapterCount(), 5);

    CHECK_EQ(checkpointChapterPart(0), 0x3E82);
    CHECK_EQ(checkpointChapterLen(0), 1);
    CHECK_EQ(strcmp(checkpointWord(0), "LDKD"), 0);

    CHECK_EQ(checkpointChapterPart(2), 0x3E84);
    CHECK_EQ(checkpointChapterLen(2), 9);
    CHECK_EQ(strcmp(checkpointWord(checkpointChapterFirst(2)), "CLLD"), 0);

    CHECK_EQ(checkpointChapterPart(3), 0x3E85);
    CHECK_EQ(checkpointChapterLen(3), 1);
    CHECK_EQ(strcmp(checkpointWord(checkpointChapterFirst(3)), "CKJL"), 0);

    CHECK_EQ(checkpointChapterPart(4), 0x3E86);
    CHECK_EQ(checkpointChapterLen(4), 3);
    CHECK_EQ(strcmp(checkpointWord(CHECKPOINT_COUNT - 1), "TXHF"), 0);
}

static void test_the_table_is_sorted_by_ordinal()
{
    for (int i = 1; i < CHECKPOINT_COUNT; ++i) {
        CHECK(checkpointOrdinal(i) > checkpointOrdinal(i - 1));
    }
}

static void test_ordinals_map_back_to_their_checkpoints()
{
    for (int i = 0; i < CHECKPOINT_COUNT; ++i) {
        CHECK_EQ(checkpointOfOrdinal(checkpointOrdinal(i)), i);
    }
    CHECK_EQ(checkpointOfOrdinal(0), -1);
    CHECK_EQ(checkpointOfOrdinal(11), -1);
    CHECK_EQ(checkpointOfOrdinal(69), -1);
}

static void test_the_long_chapter_is_in_the_games_own_order()
{
    const int first = checkpointChapterFirst(2);

    CHECK_EQ(strcmp(checkpointWord(first + 1), "LBKG"), 0);
    CHECK_EQ(strcmp(checkpointWord(first + 2), "XDDJ"), 0);
    CHECK_EQ(strcmp(checkpointWord(first + 8), "HBHK"), 0);
    CHECK_EQ(checkpointOrdinal(first), 30);
    CHECK_EQ(checkpointOrdinal(first + 8), 47);
}

int main()
{
    test_every_string_id_in_range_and_used_once();
    test_the_unreachable_words_are_absent();
    test_string_ids_map_back_to_their_checkpoints();
    test_the_sorted_table_matches_the_probe();
    test_the_table_is_sorted_by_ordinal();
    test_ordinals_map_back_to_their_checkpoints();
    test_the_long_chapter_is_in_the_games_own_order();
    test_every_word_is_four_letters();
    test_chapters_partition_the_table_in_order();
    test_chapter_parts_are_reachable_and_ascending();
    test_chapter_of_agrees_with_the_ranges();

    if (g_fail == 0) {
        printf("all tests passed\n");
        return 0;
    }
    printf("%d check(s) failed\n", g_fail);
    return 1;
}
