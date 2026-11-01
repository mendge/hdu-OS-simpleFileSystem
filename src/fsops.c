#include <string.h>
#include <time.h>

#include "bitmap.h"
#include "dir.h"
#include "disk.h"
#include "fsops.h"
#include "inode.h"
#include "user.h"

const char *fs_strerror(int err)
{
    switch (err) {
    case FS_OK:      return "success";
    case E_NOENT:    return "no such file or directory";
    case E_EXIST:    return "file exists";
    case E_NOTDIR:   return "not a directory";
    case E_ISDIR:    return "is a directory";
    case E_NOTEMPTY: return "directory not empty (use rm -r)";
    case E_NOSPC:    return "no space left on device";
    case E_NOINODE:  return "no free inode";
    case E_FBIG:     return "file too large";
    case E_ACCES:    return "permission denied";
    case E_PERM:     return "operation not permitted";
    case E_NAME:     return "invalid name";
    case E_INVAL:    return "invalid argument";
    case E_BUSY:     return "resource busy (root or current directory)";
    case E_BADFS:    return "not a valid disk image";
    case E_IO:       return "cannot open disk image";
    default:         return "unknown error";
    }
}

/* ---------------------------------------------------------------- 格式化与挂载 */

static superblock_t expected_super(void)
{
    superblock_t sb = {
        .magic = FS_MAGIC,
        .block_size = BLOCK_SIZE,
        .nblocks = NBLOCKS,
        .ninodes = NINODES,
        .bitmap_block = BITMAP_BLOCK,
        .user_block = USER_BLOCK,
        .inode_start = INODE_START,
        .data_start = DATA_START,
    };
    return sb;
}

int fs_format(const char *image)
{
    uint8_t blk[BLOCK_SIZE] = {0};
    superblock_t sb = expected_super();
    uint32_t root;
    int r;

    if ((r = disk_create(image)) != FS_OK)      /* 新镜像全为 0，inode 表自然全是空闲 */
        return r;
    memcpy(blk, &sb, sizeof sb);
    disk_write(SUPER_BLOCK, blk);
    bitmap_format();
    users_format();

    root = ialloc(T_DIR, 0755, ROOT_UID, 0);
    if (root != ROOT_INUM)
        panic("fs_format: root inode is not ROOT_INUM");
    dir_link(root, ".", root);
    dir_link(root, "..", root);                 /* 根目录的父目录是它自己 */
    return FS_OK;
}

int fs_mount(const char *image)
{
    uint8_t blk[BLOCK_SIZE];
    superblock_t sb, want = expected_super();
    int r;

    if ((r = disk_open(image)) != FS_OK)
        return r;
    disk_read(SUPER_BLOCK, blk);
    memcpy(&sb, blk, sizeof sb);
    if (memcmp(&sb, &want, sizeof sb) != 0) {
        disk_close();
        return E_BADFS;
    }
    return FS_OK;
}

void fs_unmount(void)
{
    disk_close();
}

/* ---------------------------------------------------------------- 权限与路径 */

int fs_permitted(const session_t *s, const inode_t *ip, int want)
{
    int bits;

    if (s->uid == ROOT_UID)
        return 1;                                   /* root 不受权限位限制 */
    if (s->uid == ip->uid)
        bits = ip->mode >> 6;                       /* 所有者 */
    else if (s->gid == ip->gid)
        bits = ip->mode >> 3;                       /* 同组 */
    else
        bits = ip->mode;                            /* 其他人 */
    return (bits & want & 7) == want;
}

/*
 * 逐级解析路径。注意：这里自己切分路径而不用 strtok，
 * 因为 strtok 会原地修改字符串，且有隐藏的全局状态。
 * 经过的每一级目录都需要 x（搜索）权限。
 */
int fs_lookup(const session_t *s, const char *path, uint32_t *inum)
{
    uint32_t cur;
    const char *p = path;

    if (*path == '\0')
        return E_NOENT;
    if (strlen(path) > PATH_MAX_LEN)
        return E_NAME;
    cur = (*path == '/') ? ROOT_INUM : s->cwd;

    while (*p) {
        char name[NAME_MAX_LEN + 1];
        size_t len;
        inode_t ino;
        int r;

        while (*p == '/')
            p++;
        if (*p == '\0')
            break;
        len = strcspn(p, "/");
        if (len > NAME_MAX_LEN)
            return E_NAME;
        memcpy(name, p, len);
        name[len] = '\0';
        p += len;

        iread(cur, &ino);
        if (ino.type != T_DIR)
            return E_NOTDIR;
        if (!fs_permitted(s, &ino, PERM_X))
            return E_ACCES;
        if ((r = dir_lookup(cur, name, &cur)) != FS_OK)
            return r;
    }
    *inum = cur;
    return FS_OK;
}

int fs_open(const session_t *s, const char *path, int want, uint32_t *inum, inode_t *ino)
{
    int r = fs_lookup(s, path, inum);

    if (r != FS_OK)
        return r;
    iread(*inum, ino);
    return fs_permitted(s, ino, want) ? FS_OK : E_ACCES;
}

/* 把 "a/b/c" 拆成父目录 "a/b" 的 inode 号和最后一级名字 "c" */
static int lookup_parent(const session_t *s, const char *path, uint32_t *parent, char name[NAME_MAX_LEN + 1])
{
    char buf[PATH_MAX_LEN + 1];
    const char *dirpath, *base;
    char *slash;
    size_t len = strlen(path);
    inode_t ino;
    int r;

    if (len == 0)
        return E_NOENT;
    if (len > PATH_MAX_LEN)
        return E_NAME;
    strcpy(buf, path);
    while (len > 1 && buf[len - 1] == '/')          /* 去掉末尾多余的 '/' */
        buf[--len] = '\0';

    slash = strrchr(buf, '/');
    base = slash ? slash + 1 : buf;
    if (strlen(base) > NAME_MAX_LEN)
        return E_NAME;
    strcpy(name, base);

    if (!slash) {
        dirpath = ".";
    } else if (slash == buf) {
        dirpath = "/";
    } else {
        *slash = '\0';
        dirpath = buf;
    }
    if ((r = fs_lookup(s, dirpath, parent)) != FS_OK)
        return r;
    iread(*parent, &ino);
    return ino.type == T_DIR ? FS_OK : E_NOTDIR;
}

/* target 是否是 dir 本身或它的某一级祖先 */
static int is_ancestor(uint32_t target, uint32_t dir)
{
    for (;;) {
        if (dir == target)
            return 1;
        if (dir == ROOT_INUM)
            return 0;
        dir_lookup(dir, "..", &dir);
    }
}

/* ---------------------------------------------------------------- 创建 */

static int create_node(const session_t *s, const char *path, uint16_t type, uint16_t mode, uint32_t *out)
{
    uint32_t parent, inum;
    char name[NAME_MAX_LEN + 1];
    inode_t pino;
    int r;

    if ((r = lookup_parent(s, path, &parent, name)) != FS_OK)
        return r;
    if ((r = dir_name_valid(name)) != FS_OK)
        return r;
    iread(parent, &pino);
    if (!fs_permitted(s, &pino, PERM_W | PERM_X))   /* 在目录里加一项 = 修改这个目录 */
        return E_ACCES;
    if (dir_lookup(parent, name, NULL) == FS_OK)
        return E_EXIST;

    inum = ialloc(type, mode, s->uid, s->gid);
    if (inum == 0)
        return E_NOINODE;
    if (type == T_DIR &&
        ((r = dir_link(inum, ".", inum)) != FS_OK || (r = dir_link(inum, "..", parent)) != FS_OK)) {
        ifree(inum);                                /* 失败时回滚，不留下半成品 */
        return r;
    }
    if ((r = dir_link(parent, name, inum)) != FS_OK) {
        ifree(inum);
        return r;
    }
    if (out)
        *out = inum;
    return FS_OK;
}

int fs_mkdir(const session_t *s, const char *path)
{
    return create_node(s, path, T_DIR, 0755, NULL);
}

int fs_touch(const session_t *s, const char *path)
{
    uint32_t inum;
    inode_t ino;
    int r = fs_open(s, path, PERM_W, &inum, &ino);

    if (r == E_NOENT)
        return create_node(s, path, T_FILE, 0644, NULL);
    if (r != FS_OK)
        return r;
    ino.mtime = (uint32_t)time(NULL);               /* 已存在：只更新修改时间 */
    iwrite(inum, &ino);
    return FS_OK;
}

/* ---------------------------------------------------------------- 写文件 */

int fs_write(const session_t *s, const char *path, const void *data, uint32_t len, int append)
{
    uint32_t inum, off;
    inode_t ino;
    int r;

    if (len > MAX_FILE_SIZE)
        return E_FBIG;
    r = fs_lookup(s, path, &inum);
    if (r == E_NOENT)
        r = create_node(s, path, T_FILE, 0644, &inum);     /* 不存在就创建 */
    if (r != FS_OK)
        return r;

    iread(inum, &ino);
    if (ino.type == T_DIR)
        return E_ISDIR;
    if (!fs_permitted(s, &ino, PERM_W))
        return E_ACCES;
    off = append ? ino.size : 0;
    if ((uint64_t)off + len > MAX_FILE_SIZE)        /* 先检查再截断，避免毁掉原文件 */
        return E_FBIG;
    if (!append)
        itrunc(inum);                               /* 覆盖写：先释放旧数据块 */

    r = writei(inum, data, off, len);
    if (r < 0)
        return r;
    return (uint32_t)r == len ? FS_OK : E_NOSPC;
}

/* ---------------------------------------------------------------- 删除 */

static int remove_entry(const session_t *s, uint32_t parent, const char *name, uint32_t inum, int recursive)
{
    inode_t pino, ino;
    dirent_t de;
    int r;

    iread(parent, &pino);
    iread(inum, &ino);
    if (!fs_permitted(s, &pino, PERM_W | PERM_X))
        return E_ACCES;

    if (ino.type == T_DIR && !dir_is_empty(inum)) {
        if (!recursive)
            return E_NOTEMPTY;
        /* 删除只是把槽位清零、不移动其他项，所以可以边遍历边删 */
        for (uint32_t idx = 0; dir_read_entry(inum, idx, &de); idx++) {
            if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;
            if ((r = remove_entry(s, inum, de.name, de.inum, 1)) != FS_OK)
                return r;
        }
    }
    dir_unlink(parent, name);
    ifree(inum);
    return FS_OK;
}

int fs_remove(const session_t *s, const char *path, int recursive)
{
    uint32_t inum, parent;
    char name[NAME_MAX_LEN + 1];
    int r;

    if ((r = fs_lookup(s, path, &inum)) != FS_OK)
        return r;
    if (inum == ROOT_INUM || is_ancestor(inum, s->cwd))
        return E_BUSY;                              /* 不能删掉自己脚下的目录 */
    if ((r = lookup_parent(s, path, &parent, name)) != FS_OK)
        return r;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return E_INVAL;
    return remove_entry(s, parent, name, inum, recursive);
}

/* ---------------------------------------------------------------- 目录导航 */

int fs_chdir(session_t *s, const char *path)
{
    uint32_t inum;
    inode_t ino;
    int r = fs_lookup(s, path, &inum);

    if (r != FS_OK)
        return r;
    iread(inum, &ino);
    if (ino.type != T_DIR)
        return E_NOTDIR;
    if (!fs_permitted(s, &ino, PERM_X))
        return E_ACCES;
    s->cwd = inum;
    return FS_OK;
}

/* 沿着 .. 一路走到根目录，在每一级父目录里反查自己的名字，拼出完整路径 */
int fs_getcwd(const session_t *s, char *buf, uint32_t size)
{
    char tmp[PATH_MAX_LEN + 1];
    uint32_t pos = sizeof tmp - 1;
    uint32_t cur = s->cwd;

    tmp[pos] = '\0';
    while (cur != ROOT_INUM) {
        char name[NAME_MAX_LEN + 1];
        uint32_t parent, len;

        dir_lookup(cur, "..", &parent);
        if (dir_name_of(parent, cur, name) != FS_OK)
            return E_NOENT;
        len = (uint32_t)strlen(name);
        if (len + 1 > pos)
            return E_NAME;
        pos -= len;
        memcpy(tmp + pos, name, len);
        tmp[--pos] = '/';
        cur = parent;
    }
    if (pos == sizeof tmp - 1)
        tmp[--pos] = '/';
    if (sizeof tmp - pos > size)
        return E_NAME;
    strcpy(buf, tmp + pos);
    return FS_OK;
}

/* ---------------------------------------------------------------- 属性 */

int fs_chmod(const session_t *s, const char *path, uint16_t mode)
{
    uint32_t inum;
    inode_t ino;
    int r;

    if (mode > 0777)
        return E_INVAL;
    if ((r = fs_lookup(s, path, &inum)) != FS_OK)
        return r;
    iread(inum, &ino);
    if (s->uid != ROOT_UID && s->uid != ino.uid)    /* 只有所有者和 root 能改权限 */
        return E_PERM;
    ino.mode = mode;
    iwrite(inum, &ino);
    return FS_OK;
}

void fs_reassign_owner(uint16_t from_uid, uint16_t to_uid, uint16_t to_gid)
{
    for (uint32_t inum = 1; inum < NINODES; inum++) {
        inode_t ino;

        iread(inum, &ino);
        if (ino.type != T_FREE && ino.uid == from_uid) {
            ino.uid = to_uid;
            ino.gid = to_gid;
            iwrite(inum, &ino);
        }
    }
}
