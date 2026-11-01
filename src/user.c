#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "disk.h"
#include "user.h"

static const char *const group_names[NGROUPS] = { "root", "staff", "guest" };

const char *group_name(uint16_t gid)
{
    return gid < NGROUPS ? group_names[gid] : "?";
}

int group_find(const char *name)
{
    for (int gid = 0; gid < NGROUPS; gid++)
        if (strcmp(group_names[gid], name) == 0)
            return gid;
    return E_NOENT;
}

static void load_users(user_t users[MAX_USERS])
{
    uint8_t blk[BLOCK_SIZE];

    disk_read(USER_BLOCK, blk);
    memcpy(users, blk, sizeof(user_t) * MAX_USERS);
}

static void store_users(const user_t users[MAX_USERS])
{
    uint8_t blk[BLOCK_SIZE] = {0};

    memcpy(blk, users, sizeof(user_t) * MAX_USERS);
    disk_write(USER_BLOCK, blk);
}

void users_format(void)
{
    user_t users[MAX_USERS];

    memset(users, 0, sizeof users);
    users[ROOT_UID].used = 1;
    users[ROOT_UID].gid = 0;
    strcpy(users[ROOT_UID].name, "root");
    strcpy(users[ROOT_UID].password, "123456");
    store_users(users);
}

static int find_by_name(const user_t users[MAX_USERS], const char *name)
{
    for (int uid = 0; uid < MAX_USERS; uid++)
        if (users[uid].used && strcmp(users[uid].name, name) == 0)
            return uid;
    return E_NOENT;
}

int user_auth(const char *name, const char *password, uint16_t *uid, uint16_t *gid)
{
    user_t users[MAX_USERS];
    int u;

    load_users(users);
    u = find_by_name(users, name);
    if (u < 0 || strcmp(users[u].password, password) != 0)
        return E_ACCES;
    *uid = (uint16_t)u;
    *gid = users[u].gid;
    return FS_OK;
}

int user_find(const char *name)
{
    user_t users[MAX_USERS];

    load_users(users);
    return find_by_name(users, name);
}

int user_get(uint16_t uid, user_t *out)
{
    user_t users[MAX_USERS];

    if (uid >= MAX_USERS)
        return E_NOENT;
    load_users(users);
    if (!users[uid].used)
        return E_NOENT;
    *out = users[uid];
    return FS_OK;
}

void user_name(uint16_t uid, char buf[USERNAME_LEN + 1])
{
    user_t u;

    if (user_get(uid, &u) == FS_OK)
        strcpy(buf, u.name);
    else
        snprintf(buf, USERNAME_LEN + 1, "%u", (unsigned)uid);
}

/* 用户名：1~15 个字母、数字或下划线 */
static int username_valid(const char *name)
{
    size_t len = strlen(name);

    if (len == 0 || len > USERNAME_LEN)
        return 0;
    for (size_t i = 0; i < len; i++)
        if (!isalnum((unsigned char)name[i]) && name[i] != '_')
            return 0;
    return 1;
}

int user_add(const char *name, const char *password, uint16_t gid)
{
    user_t users[MAX_USERS];
    size_t pwlen = strlen(password);

    if (!username_valid(name))
        return E_NAME;
    if (pwlen == 0 || pwlen > PASSWORD_LEN || gid >= NGROUPS)
        return E_INVAL;
    load_users(users);
    if (find_by_name(users, name) >= 0)
        return E_EXIST;
    for (int uid = 0; uid < MAX_USERS; uid++) {
        if (users[uid].used)
            continue;
        memset(&users[uid], 0, sizeof users[uid]);
        users[uid].used = 1;
        users[uid].gid = (uint8_t)gid;
        strcpy(users[uid].name, name);
        strcpy(users[uid].password, password);
        store_users(users);
        return uid;
    }
    return E_NOSPC;
}

int user_del(const char *name)
{
    user_t users[MAX_USERS];
    int uid;

    load_users(users);
    uid = find_by_name(users, name);
    if (uid < 0)
        return uid;
    if (uid == ROOT_UID)
        return E_PERM;
    memset(&users[uid], 0, sizeof users[uid]);
    store_users(users);
    return uid;
}
