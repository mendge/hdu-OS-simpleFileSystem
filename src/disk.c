#include <stdio.h>
#include <stdlib.h>

#include "disk.h"
#include "fs.h"

static FILE *disk;

void panic(const char *msg)
{
    fflush(stdout);
    fprintf(stderr, "panic: %s\n", msg);
    exit(1);
}

int disk_create(const char *path)
{
    static const uint8_t zero[BLOCK_SIZE];

    /* 必须用二进制模式 "b"：文本模式在 Windows 上会把 \n 改写成 \r\n，镜像就坏了 */
    disk = fopen(path, "w+b");
    if (!disk)
        return E_IO;
    for (uint32_t i = 0; i < NBLOCKS; i++) {
        if (fwrite(zero, BLOCK_SIZE, 1, disk) != 1) {
            disk_close();
            return E_IO;
        }
    }
    fflush(disk);
    return FS_OK;
}

int disk_open(const char *path)
{
    disk = fopen(path, "r+b");
    if (!disk)
        return E_IO;
    /* 大小不对的文件一定不是我们的镜像 */
    if (fseek(disk, 0, SEEK_END) != 0 || ftell(disk) != (long)NBLOCKS * BLOCK_SIZE) {
        disk_close();
        return E_BADFS;
    }
    return FS_OK;
}

void disk_close(void)
{
    if (disk) {
        fclose(disk);
        disk = NULL;
    }
}

void disk_read(uint32_t bno, void *buf)
{
    if (!disk || bno >= NBLOCKS)
        panic("disk_read: bad block number");
    if (fseek(disk, (long)bno * BLOCK_SIZE, SEEK_SET) != 0 || fread(buf, BLOCK_SIZE, 1, disk) != 1)
        panic("disk_read: I/O error");
}

void disk_write(uint32_t bno, const void *buf)
{
    if (!disk || bno >= NBLOCKS)
        panic("disk_write: bad block number");
    if (fseek(disk, (long)bno * BLOCK_SIZE, SEEK_SET) != 0 || fwrite(buf, BLOCK_SIZE, 1, disk) != 1)
        panic("disk_write: I/O error");
    fflush(disk);
}
