/*
 * dir.h —— 目录：目录也是文件，内容是一串定长的 dirent_t
 *
 * 每个目录都含有 "." （指向自己）和 ".." （指向父目录）两项。
 * 目录项只记录 名字 -> inode 号，文件的元数据只在 inode 里存一份，
 * 因此改动文件属性时不需要同步任何目录。
 */
#ifndef DIR_H
#define DIR_H

#include <stdint.h>

#include "fs.h"

int dir_name_valid(const char *name);     /* 新建文件时用：非空、不含 '/'、不是 "." ".."、不超长 */

/* 读第 idx 个目录项（可能是空槽 inum==0）；读到末尾返回 0 */
int dir_read_entry(uint32_t dir, uint32_t idx, dirent_t *de);

int dir_lookup(uint32_t dir, const char *name, uint32_t *inum);     /* 找不到返回 E_NOENT */
int dir_link(uint32_t dir, const char *name, uint32_t inum);        /* 添加目录项 */
int dir_unlink(uint32_t dir, const char *name);                     /* 删除目录项 */
int dir_is_empty(uint32_t dir);                                     /* 只有 . 和 .. */
int dir_name_of(uint32_t dir, uint32_t inum, char name[NAME_MAX_LEN + 1]); /* 反查名字 */

#endif
