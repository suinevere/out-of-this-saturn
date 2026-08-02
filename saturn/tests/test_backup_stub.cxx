#include <cstdio>
#include <cstdint>
#include <cstring>
#include "backup.h"

void stub_bup_reset(void);
void stub_bup_set_device(uint32_t device, int present, int formatted,
                         int writeProtected, uint32_t freeBytes);
void stub_bup_add_file(uint32_t device, const char *name, const void *data,
                       int32_t size, uint32_t date);

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

static void test_read_refuses_oversized_file(void)
{
    stub_bup_reset();
    uint8_t stored[100];
    memset(stored, 0xAB, sizeof(stored));
    stub_bup_add_file(BACKUP_INTERNAL, "AW_SAVE1", stored, sizeof(stored), 0);

    uint8_t dst[50];
    int rc = backup_read(BACKUP_INTERNAL, "AW_SAVE1", dst, sizeof(dst));
    CHECK_EQ(rc, BACKUP_ERR_BROKEN);
}

static void test_read_exact_size_succeeds(void)
{
    stub_bup_reset();
    uint8_t stored[50];
    memset(stored, 0xCD, sizeof(stored));
    stub_bup_add_file(BACKUP_INTERNAL, "AW_SAVE1", stored, sizeof(stored), 0);

    uint8_t dst[50];
    memset(dst, 0, sizeof(dst));
    int rc = backup_read(BACKUP_INTERNAL, "AW_SAVE1", dst, sizeof(dst));
    CHECK_EQ(rc, BACKUP_OK);
    CHECK_EQ(memcmp(dst, stored, sizeof(stored)), 0);
}

static void test_write_refuses_existing_without_overwrite(void)
{
    stub_bup_reset();
    uint8_t data[16];
    memset(data, 0x11, sizeof(data));

    int rc = backup_write(BACKUP_INTERNAL, "AW_SAVE1", "c", data,
                           sizeof(data), 0);
    CHECK_EQ(rc, BACKUP_OK);

    rc = backup_write(BACKUP_INTERNAL, "AW_SAVE1", "c", data, sizeof(data),
                       0);
    CHECK_EQ(rc, BACKUP_ERR_EXISTS);
}

static void test_write_allows_existing_with_overwrite(void)
{
    stub_bup_reset();
    uint8_t data[16];
    memset(data, 0x22, sizeof(data));

    int rc = backup_write(BACKUP_INTERNAL, "AW_SAVE1", "c", data,
                           sizeof(data), 0);
    CHECK_EQ(rc, BACKUP_OK);

    rc = backup_write(BACKUP_INTERNAL, "AW_SAVE1", "c", data, sizeof(data),
                       1);
    CHECK_EQ(rc, BACKUP_OK);
}

static void test_write_refuses_over_stub_capacity(void)
{
    stub_bup_reset();
    stub_bup_set_device(BACKUP_INTERNAL, 1, 1, 0, 1u << 20);

    static uint8_t big[8193];
    memset(big, 0x33, sizeof(big));

    int rc = backup_write(BACKUP_INTERNAL, "AW_SAVE1", "c", big,
                           sizeof(big), 0);
    CHECK_EQ(rc, BACKUP_ERR_NO_SPACE);
}

int main(void)
{
    test_read_refuses_oversized_file();
    test_read_exact_size_succeeds();
    test_write_refuses_existing_without_overwrite();
    test_write_allows_existing_with_overwrite();
    test_write_refuses_over_stub_capacity();

    if (g_fail == 0) {
        printf("all tests passed\n");
        return 0;
    }
    printf("%d check(s) failed\n", g_fail);
    return 1;
}
