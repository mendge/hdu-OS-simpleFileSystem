#include <string.h>

#include "dir.h"
#include "inode.h"

static int is_dot_or_dotdot(const char *name)
{
    return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
}

int dir_name_valid(const char *name)
{
    size_t len = strlen(name);

    if (len == 0 || len > NAME_MAX_LEN || strchr(name, '/') || is_dot_or_dotdot(name))
        return E_NAME;
    return FS_OK;
}

int dir_read_entry(uint32_t dir, uint32_t idx, dirent_t *de)
{
    return readi(dir, de, idx * DIRENT_SIZE, DIRENT_SIZE) == DIRENT_SIZE;
}

int dir_lookup(uint32_t dir, const char *name, uint32_t *inum)
{
    dirent_t de;

    for (uint32_t idx = 0; dir_read_entry(dir, idx, &de); idx++) {
        if (de.inum && strcmp(de.name, name) == 0) {
            if (inum)
                *inum = de.inum;
            return FS_OK;
        }
    }
    return E_NOENT;
}

int dir_link(uint32_t dir, const char *name, uint32_t inum)
{
    dirent_t de;
    uint32_t idx;
    int n;

    if (dir_lookup(dir, name, NULL) == FS_OK)
        return E_EXIST;
    /* 优先复用被删除后留下的空槽，没有就追加到目录末尾 */
    for (idx = 0; dir_read_entry(dir, idx, &de); idx++)
        if (de.inum == 0)
            break;

    memset(&de, 0, sizeof de);
    de.inum = inum;
    memcpy(de.name, name, strlen(name));        /* 调用者已保证长度合法 */
    n = writei(dir, &de, idx * DIRENT_SIZE, DIRENT_SIZE);
    if (n < 0)
        return n;
    return n == DIRENT_SIZE ? FS_OK : E_NOSPC;
}

int dir_unlink(uint32_t dir, const char *name)
{
    dirent_t de;

    for (uint32_t idx = 0; dir_read_entry(dir, idx, &de); idx++) {
        if (de.inum && strcmp(de.name, name) == 0) {
            memset(&de, 0, sizeof de);
            writei(dir, &de, idx * DIRENT_SIZE, DIRENT_SIZE);   /* 覆盖已有槽位，不会失败 */
            return FS_OK;
        }
    }
    return E_NOENT;
}

int dir_is_empty(uint32_t dir)
{
    dirent_t de;

    for (uint32_t idx = 0; dir_read_entry(dir, idx, &de); idx++)
        if (de.inum && !is_dot_or_dotdot(de.name))
            return 0;
    return 1;
}

int dir_name_of(uint32_t dir, uint32_t inum, char name[NAME_MAX_LEN + 1])
{
    dirent_t de;

    for (uint32_t idx = 0; dir_read_entry(dir, idx, &de); idx++) {
        if (de.inum == inum && !is_dot_or_dotdot(de.name)) {
            strcpy(name, de.name);
            return FS_OK;
        }
    }
    return E_NOENT;
}
