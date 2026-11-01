/*
 * fsops.h —— 面向用户的文件系统操作（相当于系统调用层）
 *
 * 这一层负责：格式化/挂载、路径解析、权限检查，以及
 * mkdir/write/rm/chdir 等完整操作。所有路径都支持绝对路径
 * （以 / 开头）和相对路径（相对当前目录），可以包含 . 和 ..。
 */
#ifndef FSOPS_H
#define FSOPS_H

#include <stdint.h>

#include "fs.h"

#define PATH_MAX_LEN 255

/* 一个登录会话：当前用户与当前工作目录 */
typedef struct {
    uint16_t uid;
    uint16_t gid;
    uint32_t cwd;
} session_t;

int  fs_format(const char *image);
int  fs_mount(const char *image);
void fs_unmount(void);

int  fs_permitted(const session_t *s, const inode_t *ip, int want);    /* want 为 PERM_* 的组合 */
int  fs_lookup(const session_t *s, const char *path, uint32_t *inum);
/* 查找并检查权限，成功时把 inode 号和内容一并返回 */
int  fs_open(const session_t *s, const char *path, int want, uint32_t *inum, inode_t *ino);

int  fs_mkdir(const session_t *s, const char *path);
int  fs_touch(const session_t *s, const char *path);
int  fs_write(const session_t *s, const char *path, const void *data, uint32_t len, int append);
int  fs_remove(const session_t *s, const char *path, int recursive);
int  fs_chdir(session_t *s, const char *path);
int  fs_getcwd(const session_t *s, char *buf, uint32_t size);
int  fs_chmod(const session_t *s, const char *path, uint16_t mode);

void fs_reassign_owner(uint16_t from_uid, uint16_t to_uid, uint16_t to_gid);

#endif
