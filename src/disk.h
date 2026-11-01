/*
 * disk.h —— 模拟磁盘：以"块"为单位读写磁盘镜像文件
 *
 * 这是唯一直接接触宿主机文件的一层。上层只能整块读写，
 * 和真实磁盘驱动的接口一致。每次写都会立即落盘（write-through），
 * 所以程序中途退出也不会丢失已经完成的操作。
 */
#ifndef DISK_H
#define DISK_H

#include <stdint.h>

int  disk_create(const char *path);   /* 新建一个全 0 的镜像 */
int  disk_open(const char *path);     /* 打开已有镜像 */
void disk_close(void);

/* 读写第 bno 块；buf 必须有 BLOCK_SIZE 字节。I/O 出错直接 panic */
void disk_read(uint32_t bno, void *buf);
void disk_write(uint32_t bno, const void *buf);

/* 不可恢复的内部错误：打印信息并退出（真实内核也是这么做的） */
void panic(const char *msg);

#endif
