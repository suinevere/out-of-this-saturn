#include <cstdio>
#include <cstdint>
#include "backup.h"

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

static uint32_t packed(int year, int month, int day, int hour, int min)
{
    static const int cum[12] = {0,31,59,90,120,151,181,212,243,273,304,334};
    int y = year - 1980;
    int days = y * 365 + (y + 3) / 4 + cum[month - 1] + (day - 1);
    if (month > 2 && (year % 4) == 0) {
        days += 1;
    }
    return (uint32_t)days * 1440u + (uint32_t)hour * 60u + (uint32_t)min;
}

static void test_epoch(void)
{
    int mo = -1, d = -1, h = -1, mi = -1;
    backup_date_split(0, &mo, &d, &h, &mi);
    CHECK_EQ(mo, 1);
    CHECK_EQ(d, 1);
    CHECK_EQ(h, 0);
    CHECK_EQ(mi, 0);
}

static void test_known_stamp(void)
{
    int mo = 0, d = 0, h = 0, mi = 0;
    backup_date_split(packed(2026, 8, 1, 21, 14), &mo, &d, &h, &mi);
    CHECK_EQ(mo, 8);
    CHECK_EQ(d, 1);
    CHECK_EQ(h, 21);
    CHECK_EQ(mi, 14);
}

static void test_leap_day(void)
{
    int mo = 0, d = 0, h = 0, mi = 0;
    backup_date_split(packed(2024, 2, 29, 12, 0), &mo, &d, &h, &mi);
    CHECK_EQ(mo, 2);
    CHECK_EQ(d, 29);
    CHECK_EQ(h, 12);
}

static void test_null_outputs_are_safe(void)
{
    backup_date_split(packed(2026, 8, 1, 21, 14), 0, 0, 0, 0);
}

int main(void)
{
    test_epoch();
    test_known_stamp();
    test_leap_day();
    test_null_outputs_are_safe();

    if (g_fail == 0) {
        printf("all tests passed\n");
        return 0;
    }
    printf("%d check(s) failed\n", g_fail);
    return 1;
}
