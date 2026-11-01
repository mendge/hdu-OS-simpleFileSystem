#include <string.h>

#include "bitmap.h"
#include "disk.h"
#include "fs.h"

static int bit_get(const uint8_t *bm, uint32_t b)
{
    return (bm[b / 8] >> (b % 8)) & 1;
}

static void bit_set(uint8_t *bm, uint32_t b, int used)
{
    if (used)
        bm[b / 8] |= (uint8_t)(1u << (b % 8));
    else
        bm[b / 8] &= (uint8_t)~(1u << (b % 8));
}

void bitmap_format(void)
{
    uint8_t bm[BLOCK_SIZE] = {0};

    for (uint32_t b = 0; b < DATA_START; b++)
        bit_set(bm, b, 1);
    disk_write(BITMAP_BLOCK, bm);
}

uint32_t balloc(void)
{
    uint8_t bm[BLOCK_SIZE];

    disk_read(BITMAP_BLOCK, bm);
    for (uint32_t b = DATA_START; b < NBLOCKS; b++) {
        if (!bit_get(bm, b)) {
            uint8_t zero[BLOCK_SIZE] = {0};

            bit_set(bm, b, 1);
            disk_write(BITMAP_BLOCK, bm);
            disk_write(b, zero);    /* 新块清零，避免读到上一个文件残留的内容 */
            return b;
        }
    }
    return 0;                       /* 0 号是超级块，不可能被分配，所以可以当"失败"用 */
}

void bfree(uint32_t bno)
{
    uint8_t bm[BLOCK_SIZE];

    if (bno < DATA_START || bno >= NBLOCKS)
        panic("bfree: not a data block");
    disk_read(BITMAP_BLOCK, bm);
    if (!bit_get(bm, bno))
        panic("bfree: block already free");
    bit_set(bm, bno, 0);
    disk_write(BITMAP_BLOCK, bm);
}

uint32_t bitmap_count_free(void)
{
    uint8_t bm[BLOCK_SIZE];
    uint32_t n = 0;

    disk_read(BITMAP_BLOCK, bm);
    for (uint32_t b = DATA_START; b < NBLOCKS; b++)
        n += !bit_get(bm, b);
    return n;
}
