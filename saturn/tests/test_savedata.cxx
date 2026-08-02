#include <cstdint>
#include "savedata.h"
#include "backup.h"

extern void stub_bup_reset(void);
extern void stub_bup_set_device(uint32_t device, int present, int formatted,
                                int writeProtected, uint32_t freeBytes);
extern void stub_bup_set_block_size(uint32_t device, uint32_t blockSize);
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

static void makeFile(uint8_t *buf, const SaveInfo *info)
{
    memset(buf, 0, SAVE_MAX_BYTES);
    savedataWriteHeader(buf, info);
}

static void test_header_round_trip(void)
{
    uint8_t buf[SAVE_HEADER_SIZE];
    SaveInfo in;

    savedataClear(&in);
    in.hasState = true;
    in.statePartId = 0x3E83;
    in.stateDate = 0x00112233;
    in.stateLen = 1234;
    in.hasCode = true;
    in.codePartId = 0x3E86;
    in.codeDate = 0x00112299;
    in.codeWord = 0x0160;

    savedataWriteHeader(buf, &in);

    uint16_t ver = 0;
    SaveInfo out;
    savedataClear(&out);
    CHECK(savedataReadHeader(buf, &ver, &out));
    CHECK_EQ(ver, 3);
    CHECK(out.hasState);
    CHECK(out.hasCode);
    CHECK_EQ(out.statePartId, 0x3E83);
    CHECK_EQ(out.stateDate, 0x00112233);
    CHECK_EQ(out.stateLen, 1234);
    CHECK_EQ(out.codePartId, 0x3E86);
    CHECK_EQ(out.codeDate, 0x00112299);
    CHECK_EQ(out.codeWord, 0x0160);
}

static void test_header_records_are_independent(void)
{
    uint8_t buf[SAVE_HEADER_SIZE];
    SaveInfo in;
    uint16_t ver = 0;
    SaveInfo out;

    savedataClear(&in);
    in.hasCode = true;
    in.codeWord = 0x0174;
    savedataWriteHeader(buf, &in);

    savedataClear(&out);
    CHECK(savedataReadHeader(buf, &ver, &out));
    CHECK(!out.hasState);
    CHECK(out.hasCode);

    savedataClear(&in);
    in.hasState = true;
    savedataWriteHeader(buf, &in);

    savedataClear(&out);
    CHECK(savedataReadHeader(buf, &ver, &out));
    CHECK(out.hasState);
    CHECK(!out.hasCode);
}

static void test_header_rejects_bad_magic(void)
{
    uint8_t buf[SAVE_HEADER_SIZE];
    SaveInfo in;
    uint16_t ver = 0;
    SaveInfo out;

    savedataClear(&in);
    savedataWriteHeader(buf, &in);
    buf[1] = 'X';

    CHECK(!savedataReadHeader(buf, &ver, &out));
}

static void test_probe_no_file(void)
{
    SaveInfo info;
    stub_bup_reset();
    CHECK_EQ(savedataProbe(BACKUP_INTERNAL, &info), SAVE_FILE_NONE);
    CHECK(!savedataHasAny(&info));
}

static void test_probe_reads_both_records(void)
{
    static uint8_t blob[SAVE_MAX_BYTES];
    SaveInfo in;
    SaveInfo info;

    stub_bup_reset();
    savedataClear(&in);
    in.hasState = true;
    in.statePartId = 0x3E84;
    in.stateDate = 100;
    in.stateLen = 64;
    in.hasCode = true;
    in.codePartId = 0x3E87;
    in.codeDate = 200;
    in.codeWord = 0x0165;
    makeFile(blob, &in);
    stub_bup_add_file(BACKUP_INTERNAL, SAVE_FILE_NAME, blob, sizeof(blob), 0);

    CHECK_EQ(savedataProbe(BACKUP_INTERNAL, &info), SAVE_FILE_OK);
    CHECK(savedataHasAny(&info));
    CHECK_EQ(info.statePartId, 0x3E84);
    CHECK_EQ(info.codeWord, 0x0165);
}

static void test_probe_rejects_bad_magic(void)
{
    static uint8_t blob[SAVE_MAX_BYTES];
    SaveInfo in;
    SaveInfo info;

    stub_bup_reset();
    savedataClear(&in);
    in.hasState = true;
    makeFile(blob, &in);
    blob[0] = 'Z';
    stub_bup_add_file(BACKUP_INTERNAL, SAVE_FILE_NAME, blob, sizeof(blob), 0);

    CHECK_EQ(savedataProbe(BACKUP_INTERNAL, &info), SAVE_FILE_DAMAGED);
    CHECK(!savedataHasAny(&info));
}

static void test_probe_rejects_old_version(void)
{
    static uint8_t blob[SAVE_MAX_BYTES];
    SaveInfo in;
    SaveInfo info;

    stub_bup_reset();
    savedataClear(&in);
    in.hasState = true;
    makeFile(blob, &in);
    blob[4] = 0;
    blob[5] = 1;
    stub_bup_add_file(BACKUP_INTERNAL, SAVE_FILE_NAME, blob, sizeof(blob), 0);

    CHECK_EQ(savedataProbe(BACKUP_INTERNAL, &info), SAVE_FILE_OLD_VERSION);
    CHECK(!savedataHasAny(&info));
}

static void test_probe_rejects_overlong_payload(void)
{
    static uint8_t blob[SAVE_MAX_BYTES];
    SaveInfo in;
    SaveInfo info;

    stub_bup_reset();
    savedataClear(&in);
    in.hasState = true;
    in.stateLen = (uint16_t)(SAVE_MAX_BYTES - SAVE_HEADER_SIZE + 1);
    makeFile(blob, &in);
    stub_bup_add_file(BACKUP_INTERNAL, SAVE_FILE_NAME, blob, sizeof(blob), 0);

    CHECK_EQ(savedataProbe(BACKUP_INTERNAL, &info), SAVE_FILE_DAMAGED);
}

static void test_newest_is_code(void)
{
    SaveInfo info;

    savedataClear(&info);
    info.hasState = true;
    info.stateDate = 500;
    CHECK(!savedataNewestIsCode(&info));

    info.hasCode = true;
    info.codeDate = 499;
    CHECK(!savedataNewestIsCode(&info));

    info.codeDate = 501;
    CHECK(savedataNewestIsCode(&info));

    info.codeDate = 500;
    CHECK(savedataNewestIsCode(&info));

    savedataClear(&info);
    info.hasCode = true;
    CHECK(savedataNewestIsCode(&info));
}

static void test_blocks_needed(void)
{
    CHECK_EQ(savedataBlocksNeeded(64, 8192), 143);
    CHECK_EQ(savedataBlocksNeeded(64, 58), 2);
    CHECK_EQ(savedataBlocksNeeded(64, 59), 3);
    CHECK_EQ(savedataBlocksNeeded(64, 0), 1);
    CHECK_EQ(savedataBlocksNeeded(4, 100), 0);
}

static void test_reserve_creates_full_size_file(void)
{
    BackupSpace space;
    BackupEntry entry;

    stub_bup_reset();
    CHECK_EQ(savedataReserve(BACKUP_INTERNAL, &space), BACKUP_OK);
    CHECK_EQ(backup_dir(BACKUP_INTERNAL, SAVE_FILE_NAME, &entry), BACKUP_OK);
    CHECK(entry.exists);
    CHECK_EQ(entry.size, SAVE_MAX_BYTES);

    SaveInfo info;
    CHECK_EQ(savedataProbe(BACKUP_INTERNAL, &info), SAVE_FILE_OK);
    CHECK(!savedataHasAny(&info));
}

static void test_reserve_is_idempotent(void)
{
    static uint8_t blob[SAVE_MAX_BYTES];
    BackupSpace space;
    SaveInfo in;
    SaveInfo info;

    stub_bup_reset();
    savedataClear(&in);
    in.hasCode = true;
    in.codeWord = 0x0170;
    makeFile(blob, &in);
    stub_bup_add_file(BACKUP_INTERNAL, SAVE_FILE_NAME, blob, sizeof(blob), 0);

    CHECK_EQ(savedataReserve(BACKUP_INTERNAL, &space), BACKUP_OK);
    CHECK_EQ(savedataProbe(BACKUP_INTERNAL, &info), SAVE_FILE_OK);
    CHECK(info.hasCode);
    CHECK_EQ(info.codeWord, 0x0170);
}

static void test_reserve_grows_and_keeps_contents(void)
{
    static uint8_t blob[SAVE_HEADER_SIZE];
    BackupSpace space;
    SaveInfo in;
    SaveInfo info;

    stub_bup_reset();
    savedataClear(&in);
    in.hasCode = true;
    in.codeWord = 0x0166;
    savedataWriteHeader(blob, &in);
    stub_bup_add_file(BACKUP_INTERNAL, SAVE_FILE_NAME, blob, sizeof(blob), 0);

    CHECK_EQ(savedataReserve(BACKUP_INTERNAL, &space), BACKUP_OK);

    BackupEntry entry;
    backup_dir(BACKUP_INTERNAL, SAVE_FILE_NAME, &entry);
    CHECK_EQ(entry.size, SAVE_MAX_BYTES);

    CHECK_EQ(savedataProbe(BACKUP_INTERNAL, &info), SAVE_FILE_OK);
    CHECK(info.hasCode);
    CHECK_EQ(info.codeWord, 0x0166);
}

static void test_reserve_reports_shortfall(void)
{
    BackupSpace space;

    stub_bup_reset();
    stub_bup_set_device(BACKUP_INTERNAL, 1, 1, 0, 40 * 64);

    CHECK_EQ(savedataReserve(BACKUP_INTERNAL, &space), BACKUP_ERR_NO_SPACE);
    CHECK_EQ(space.blockSize, 64);
    CHECK_EQ(space.freeBlocks, 40);
    CHECK_EQ(space.fits, 0);
    CHECK(savedataBlocksNeeded(space.blockSize, SAVE_MAX_BYTES) >
          space.freeBlocks);
}

static void test_reserve_refuses_absent_device(void)
{
    BackupSpace space;

    stub_bup_reset();
    CHECK_EQ(savedataReserve(BACKUP_CART, &space), BACKUP_ERR_NONE);
    CHECK_EQ(space.blockSize, 0);
}

static void test_reserve_refuses_write_protected(void)
{
    BackupSpace space;

    stub_bup_reset();
    stub_bup_set_device(BACKUP_CART, 1, 1, 1, 65536);
    CHECK_EQ(savedataReserve(BACKUP_CART, &space), BACKUP_ERR_PROTECTED);
}

static void test_chapter_names(void)
{
    CHECK(strcmp(savedataChapterName(0x3E82), "THE ARRIVAL") == 0);
    CHECK(strcmp(savedataChapterName(0x3E89), "THE FINAL") == 0);
    CHECK(strcmp(savedataChapterName(0x1234), "UNKNOWN") == 0);
}

static void test_frame_row_step(void)
{
    CHECK_EQ(savedataFrameRowStep(SAVE_FRAME_NONE), 0);
    CHECK_EQ(savedataFrameRowStep(SAVE_FRAME_RLE), 0);
    CHECK_EQ(savedataFrameRowStep(SAVE_FRAME_DELTA), 1);
    CHECK_EQ(savedataFrameRowStep(SAVE_FRAME_DELTA_H2), 2);
    CHECK_EQ(savedataFrameRowStep(SAVE_FRAME_DELTA_H4), 4);
    CHECK_EQ(savedataFrameRowStep(SAVE_FRAME_DELTA_H8), 8);
}

static void test_reached_round_trips_through_the_header()
{
    uint8_t buf[SAVE_HEADER_SIZE];
    SaveInfo in;
    SaveInfo out;
    uint16_t ver = 0;

    savedataClear(&in);
    in.hasState = true;
    in.statePartId = 0x3E85;
    in.stateLen = 64;
    in.reached = 17;

    savedataWriteHeader(buf, &in);
    CHECK(savedataReadHeader(buf, &ver, &out));
    CHECK_EQ(out.reached, 17);
    CHECK_EQ(out.statePartId, 0x3E85);
    CHECK_EQ(out.stateLen, 64);
}

static void test_a_header_without_it_reads_as_no_progress()
{
    uint8_t buf[SAVE_HEADER_SIZE];
    SaveInfo in;
    SaveInfo out;
    uint16_t ver = 0;

    savedataClear(&in);
    in.reached = 9;
    savedataWriteHeader(buf, &in);
    buf[24] = 0;

    CHECK(savedataReadHeader(buf, &ver, &out));
    CHECK_EQ(out.reached, 0);
}

static void test_clear_zeroes_reached()
{
    SaveInfo info;

    info.reached = 9;
    savedataClear(&info);
    CHECK_EQ(info.reached, 0);
}

static uint8_t pattern_byte(int i)
{
    return (uint8_t)(i * 7 + 11);
}

static void test_store_progress_leaves_payload_intact()
{
    static uint8_t blob[SAVE_MAX_BYTES];
    static uint8_t readBack[SAVE_MAX_BYTES];
    SaveInfo in;
    SaveInfo out;
    const int32_t payloadLen = 40;

    stub_bup_reset();
    savedataClear(&in);
    in.hasState = true;
    in.statePartId = 0x3E85;
    in.stateDate = 1000;
    in.stateLen = (uint16_t)payloadLen;
    in.hasCode = true;
    in.codePartId = 0x3E87;
    in.codeDate = 1200;
    in.codeWord = 0x0165;
    in.reached = 5;

    memset(blob, 0, sizeof(blob));
    savedataWriteHeader(blob, &in);
    for (int i = 0; i < payloadLen; ++i) {
        blob[SAVE_HEADER_SIZE + i] = pattern_byte(i);
    }
    stub_bup_add_file(BACKUP_INTERNAL, SAVE_FILE_NAME, blob, sizeof(blob), 500);

    CHECK_EQ(savedataStoreProgress(BACKUP_INTERNAL, 12), BACKUP_OK);

    CHECK_EQ(savedataProbe(BACKUP_INTERNAL, &out), SAVE_FILE_OK);
    CHECK(out.hasState);
    CHECK_EQ(out.statePartId, 0x3E85);
    CHECK_EQ(out.stateDate, 1000);
    CHECK_EQ(out.stateLen, payloadLen);
    CHECK(out.hasCode);
    CHECK_EQ(out.codePartId, 0x3E87);
    CHECK_EQ(out.codeDate, 1200);
    CHECK_EQ(out.codeWord, 0x0165);
    CHECK_EQ(out.reached, 12);

    CHECK_EQ(backup_read(BACKUP_INTERNAL, SAVE_FILE_NAME, readBack,
                          SAVE_MAX_BYTES), BACKUP_OK);
    CHECK_EQ(readBack[24], 12);
    int payloadMismatch = 0;
    for (int i = 0; i < payloadLen; ++i) {
        if (readBack[SAVE_HEADER_SIZE + i] != pattern_byte(i)) {
            payloadMismatch = 1;
        }
    }
    CHECK(!payloadMismatch);
}

static void test_store_progress_is_a_noop_when_unchanged()
{
    static uint8_t blob[SAVE_MAX_BYTES];
    SaveInfo in;

    stub_bup_reset();
    savedataClear(&in);
    in.hasCode = true;
    in.codeWord = 0x0170;
    in.reached = 5;
    makeFile(blob, &in);
    stub_bup_add_file(BACKUP_INTERNAL, SAVE_FILE_NAME, blob, sizeof(blob), 0);

    CHECK_EQ(savedataStoreProgress(BACKUP_INTERNAL, 5), BACKUP_OK);
}

static void test_store_progress_refuses_missing_file()
{
    stub_bup_reset();

    CHECK_EQ(savedataStoreProgress(BACKUP_INTERNAL, 5), BACKUP_ERR_NOT_FOUND);

    BackupEntry entry;
    CHECK_EQ(backup_dir(BACKUP_INTERNAL, SAVE_FILE_NAME, &entry), BACKUP_OK);
    CHECK(!entry.exists);
}

static void test_store_progress_refuses_damaged_file()
{
    static uint8_t blob[SAVE_MAX_BYTES];
    static uint8_t readBack[SAVE_MAX_BYTES];
    SaveInfo in;

    stub_bup_reset();
    savedataClear(&in);
    in.hasState = true;
    in.reached = 5;
    makeFile(blob, &in);
    blob[0] = 'Z';
    stub_bup_add_file(BACKUP_INTERNAL, SAVE_FILE_NAME, blob, sizeof(blob), 0);

    CHECK_EQ(savedataStoreProgress(BACKUP_INTERNAL, 12), BACKUP_ERR_BROKEN);

    CHECK_EQ(backup_read(BACKUP_INTERNAL, SAVE_FILE_NAME, readBack,
                          SAVE_MAX_BYTES), BACKUP_OK);
    CHECK(memcmp(readBack, blob, sizeof(blob)) == 0);
}

int main(void)
{
    test_header_round_trip();
    test_header_records_are_independent();
    test_header_rejects_bad_magic();
    test_probe_no_file();
    test_probe_reads_both_records();
    test_probe_rejects_bad_magic();
    test_probe_rejects_old_version();
    test_probe_rejects_overlong_payload();
    test_newest_is_code();
    test_blocks_needed();
    test_reserve_creates_full_size_file();
    test_reserve_is_idempotent();
    test_reserve_grows_and_keeps_contents();
    test_reserve_reports_shortfall();
    test_reserve_refuses_absent_device();
    test_reserve_refuses_write_protected();
    test_chapter_names();
    test_frame_row_step();
    test_reached_round_trips_through_the_header();
    test_a_header_without_it_reads_as_no_progress();
    test_clear_zeroes_reached();
    test_store_progress_leaves_payload_intact();
    test_store_progress_is_a_noop_when_unchanged();
    test_store_progress_refuses_missing_file();
    test_store_progress_refuses_damaged_file();

    if (g_fail == 0) {
        printf("savedata: all tests passed\n");
        return 0;
    }
    printf("savedata: %d failure(s)\n", g_fail);
    return 1;
}
