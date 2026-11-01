#include <string.h>
#include <time.h>

#include "bitmap.h"
#include "disk.h"
#include "inode.h"

/* inode 号 -> (所在块号, 块内下标) */
static void inode_pos(uint32_t inum, uint32_t *bno, uint32_t *idx)
{
    if (inum == 0 || inum >= NINODES)
        panic("bad inode number");
    *bno = INODE_START + inum / INODES_PER_BLOCK;
    *idx = inum % INODES_PER_BLOCK;
}

void iread(uint32_t inum, inode_t *ip)
{
    inode_t table[INODES_PER_BLOCK];
    uint32_t bno, idx;

    inode_pos(inum, &bno, &idx);
    disk_read(bno, table);
    *ip = table[idx];
}

void iwrite(uint32_t inum, const inode_t *ip)
{
    inode_t table[INODES_PER_BLOCK];
    uint32_t bno, idx;

    inode_pos(inum, &bno, &idx);
    disk_read(bno, table);
    table[idx] = *ip;
    disk_write(bno, table);
}

uint32_t ialloc(uint16_t type, uint16_t mode, uint16_t uid, uint16_t gid)
{
    for (uint32_t inum = 1; inum < NINODES; inum++) {
        inode_t ino;

        iread(inum, &ino);
        if (ino.type != T_FREE)
            continue;
        memset(&ino, 0, sizeof ino);
        ino.type = type;
        ino.mode = mode;
        ino.uid = uid;
        ino.gid = gid;
        ino.ctime = ino.mtime = (uint32_t)time(NULL);
        iwrite(inum, &ino);
        return inum;
    }
    return 0;
}

/*
 * 把文件内的逻辑块号 fbn 映射成磁盘块号。
 * alloc 为真时，缺少的数据块/间接块会被分配出来。
 * 返回 0 表示该块不存在（或磁盘已满分配失败）。
 */
static uint32_t bmap(inode_t *ip, uint32_t fbn, int alloc)
{
    uint32_t table[NINDIRECT];

    if (fbn < NDIRECT) {
        if (ip->direct[fbn] == 0 && alloc)
            ip->direct[fbn] = balloc();
        return ip->direct[fbn];
    }

    fbn -= NDIRECT;
    if (fbn >= NINDIRECT)
        return 0;
    if (ip->indirect == 0) {
        if (!alloc || (ip->indirect = balloc()) == 0)
            return 0;
    }
    disk_read(ip->indirect, table);
    if (table[fbn] == 0 && alloc) {
        table[fbn] = balloc();
        if (table[fbn] != 0)
            disk_write(ip->indirect, table);
    }
    return table[fbn];
}

int readi(uint32_t inum, void *dst, uint32_t off, uint32_t n)
{
    inode_t ino;
    uint8_t buf[BLOCK_SIZE];
    uint32_t done = 0;

    iread(inum, &ino);
    if (off >= ino.size)
        return 0;
    if (n > ino.size - off)
        n = ino.size - off;

    while (done < n) {
        uint32_t pos = off + done;
        uint32_t boff = pos % BLOCK_SIZE;
        uint32_t chunk = BLOCK_SIZE - boff;
        uint32_t bno = bmap(&ino, pos / BLOCK_SIZE, 0);

        if (chunk > n - done)
            chunk = n - done;
        if (bno) {
            disk_read(bno, buf);
            memcpy((uint8_t *)dst + done, buf + boff, chunk);
        } else {
            memset((uint8_t *)dst + done, 0, chunk);    /* 没分配的块（空洞）读出来是 0 */
        }
        done += chunk;
    }
    return (int)done;
}

int writei(uint32_t inum, const void *src, uint32_t off, uint32_t n)
{
    inode_t ino;
    uint8_t buf[BLOCK_SIZE];
    uint32_t done = 0;

    if ((uint64_t)off + n > MAX_FILE_SIZE)
        return E_FBIG;
    iread(inum, &ino);

    while (done < n) {
        uint32_t pos = off + done;
        uint32_t boff = pos % BLOCK_SIZE;
        uint32_t chunk = BLOCK_SIZE - boff;
        uint32_t bno = bmap(&ino, pos / BLOCK_SIZE, 1);

        if (bno == 0)
            break;                              /* 磁盘满了 */
        if (chunk > n - done)
            chunk = n - done;
        if (chunk < BLOCK_SIZE)
            disk_read(bno, buf);                /* 只改块的一部分：先读出原内容 */
        memcpy(buf + boff, (const uint8_t *)src + done, chunk);
        disk_write(bno, buf);
        done += chunk;
    }

    if (off + done > ino.size)
        ino.size = off + done;
    ino.mtime = (uint32_t)time(NULL);
    iwrite(inum, &ino);                         /* bmap 可能改了块号，必须写回 */
    return (int)done;
}

void itrunc(uint32_t inum)
{
    inode_t ino;

    iread(inum, &ino);
    for (int i = 0; i < NDIRECT; i++) {
        if (ino.direct[i]) {
            bfree(ino.direct[i]);
            ino.direct[i] = 0;
        }
    }
    if (ino.indirect) {
        uint32_t table[NINDIRECT];

        disk_read(ino.indirect, table);
        for (uint32_t i = 0; i < NINDIRECT; i++)
            if (table[i])
                bfree(table[i]);
        bfree(ino.indirect);
        ino.indirect = 0;
    }
    ino.size = 0;
    ino.mtime = (uint32_t)time(NULL);
    iwrite(inum, &ino);
}

void ifree(uint32_t inum)
{
    inode_t ino;

    itrunc(inum);
    memset(&ino, 0, sizeof ino);                /* type 变回 T_FREE */
    iwrite(inum, &ino);
}

uint32_t inode_count_blocks(const inode_t *ip)
{
    uint32_t n = 0;

    for (int i = 0; i < NDIRECT; i++)
        n += ip->direct[i] != 0;
    if (ip->indirect) {
        uint32_t table[NINDIRECT];

        disk_read(ip->indirect, table);
        for (uint32_t i = 0; i < NINDIRECT; i++)
            n += table[i] != 0;
    }
    return n;
}

uint32_t inode_count_free(void)
{
    uint32_t n = 0;

    for (uint32_t inum = 1; inum < NINODES; inum++) {
        inode_t ino;

        iread(inum, &ino);
        n += ino.type == T_FREE;
    }
    return n;
}
