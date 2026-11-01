/*
 * user.h —— 用户与组
 *
 * 用户表整体放在 USER_BLOCK 一个块里，uid 就是用户在表中的下标，
 * 删除用户只把槽位标记为未用，其他用户的 uid 不会变化。
 * 组是固定的三个：root(0) staff(1) guest(2)。
 */
#ifndef USER_H
#define USER_H

#include <stdint.h>

#include "fs.h"

#define NGROUPS 3

const char *group_name(uint16_t gid);
int         group_find(const char *name);          /* 返回 gid 或 E_NOENT */

void users_format(void);                           /* 格式化时创建 root/123456 */
int  user_auth(const char *name, const char *password, uint16_t *uid, uint16_t *gid);
int  user_find(const char *name);                  /* 返回 uid 或 E_NOENT */
int  user_get(uint16_t uid, user_t *out);          /* 槽位未用返回 E_NOENT */
void user_name(uint16_t uid, char buf[USERNAME_LEN + 1]);   /* 已删除的用户显示为 uid 数字 */
int  user_add(const char *name, const char *password, uint16_t gid);   /* 返回新 uid */
int  user_del(const char *name);                   /* 返回被删用户的 uid */

#endif
