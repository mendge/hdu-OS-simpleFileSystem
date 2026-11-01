/*
 * inode.h —— inode 的分配、读写，以及按字节偏移读写文件内容
 *
 * 文件内容的寻址采用"混合索引"：
 *   逻辑块 0..9      -> inode.direct[0..9]            （直接块）
 *   逻辑块 10..265   -> 间接块中的第 0..255 项         （一级间接）
 *
 * readi/writei 屏蔽了这些细节，上层只需要说"从第 off 字节开始读写 n 字节"。
 * 它们都以 inode 号为参数，自己负责把 inode 读进来、改完后写回去。
 */
#ifndef INODE_H
#define INODE_H

#include <stdint.h>

#include "fs.h"

void     iread(uint32_t inum, inode_t *ip);
void     iwrite(uint32_t inum, const inode_t *ip);

/* 分配一个空闲 inode 并初始化，返回 inode 号；没有空闲 inode 返回 0 */
uint32_t ialloc(uint16_t type, uint16_t mode, uint16_t uid, uint16_t gid);
void     ifree(uint32_t inum);          /* 释放全部数据块并清空 inode */
void     itrunc(uint32_t inum);         /* 释放全部数据块，大小置 0 */

/* 返回实际读到的字节数（读到文件末尾会变少） */
int      readi(uint32_t inum, void *dst, uint32_t off, uint32_t n);
/* 返回实际写入的字节数；磁盘满时会少于 n；超过最大文件大小返回 E_FBIG */
int      writei(uint32_t inum, const void *src, uint32_t off, uint32_t n);

uint32_t inode_count_blocks(const inode_t *ip);   /* 数据块个数（不含间接块本身） */
uint32_t inode_count_free(void);

#endif
