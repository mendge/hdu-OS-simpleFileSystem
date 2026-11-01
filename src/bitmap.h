/*
 * bitmap.h —— 位示图：管理数据块的分配与回收
 *
 * 位示图放在 BITMAP_BLOCK 中，第 b 位表示第 b 块是否已被占用。
 * 元数据块（超级块、位示图、用户表、inode 表）在格式化时就标记为已用。
 */
#ifndef BITMAP_H
#define BITMAP_H

#include <stdint.h>

void     bitmap_format(void);
uint32_t balloc(void);              /* 分配一个清零的数据块，返回块号；没有空闲块返回 0 */
void     bfree(uint32_t bno);       /* 释放数据块 */
uint32_t bitmap_count_free(void);

#endif
