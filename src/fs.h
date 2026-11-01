/*
 * fs.h —— 磁盘布局、磁盘上的数据结构与错误码
 *
 * 整个文件系统保存在一个普通文件（磁盘镜像，默认 disk.img）中。
 * 镜像被切成 NBLOCKS 个 BLOCK_SIZE 字节的块，布局固定如下：
 *
 *   块号:   0        1        2        3 ... 10        11 ... 1023
 *        +--------+--------+--------+-------------+-----------------+
 *        | 超级块  | 位示图  | 用户表  |  inode 表   |     数据块       |
 *        +--------+--------+--------+-------------+-----------------+
 *
 * 所有结构体都按原样（本机字节序）写入磁盘，所以每个结构体的大小
 * 都用 _Static_assert 固定下来，改字段时编译器会提醒你同步修改常量。
 */
#ifndef FS_H
#define FS_H

#include <stdint.h>

/* ---------- 磁盘几何 ---------- */
#define BLOCK_SIZE   1024                 /* 每块字节数 */
#define NBLOCKS      1024                 /* 总块数，镜像大小 = 1 MiB */

/* ---------- 磁盘布局（块号） ---------- */
#define SUPER_BLOCK  0
#define BITMAP_BLOCK 1                    /* 1 bit 对应 1 个块：1 已用，0 空闲 */
#define USER_BLOCK   2
#define INODE_START  3
#define NINODES      128                  /* inode 总数（0 号保留不用） */
#define INODE_SIZE   64
#define INODES_PER_BLOCK (BLOCK_SIZE / INODE_SIZE)          /* 16 */
#define INODE_BLOCKS     (NINODES / INODES_PER_BLOCK)       /* 8  */
#define DATA_START       (INODE_START + INODE_BLOCKS)       /* 11 */

#define FS_MAGIC     0x46554448u          /* "HDUF"，用来识别合法镜像 */
#define ROOT_INUM    1                    /* 根目录的 inode 号；0 表示"空" */
#define ROOT_UID     0

/* ---------- 超级块：描述整个文件系统的参数 ---------- */
typedef struct {
    uint32_t magic;
    uint32_t block_size;
    uint32_t nblocks;
    uint32_t ninodes;
    uint32_t bitmap_block;
    uint32_t user_block;
    uint32_t inode_start;
    uint32_t data_start;
} superblock_t;

/* ---------- inode：一个文件/目录的全部元数据 ---------- */
#define NDIRECT       10                              /* 直接块个数 */
#define NINDIRECT     (BLOCK_SIZE / sizeof(uint32_t)) /* 一个间接块能存的块号数：256 */
#define MAX_FILE_BLOCKS (NDIRECT + NINDIRECT)
#define MAX_FILE_SIZE   (MAX_FILE_BLOCKS * BLOCK_SIZE) /* 266 KiB */

enum { T_FREE = 0, T_DIR = 1, T_FILE = 2 };

/* 权限位，与 Unix 相同：所有者/同组/其他人 各 3 位 rwx */
#define PERM_R 4
#define PERM_W 2
#define PERM_X 1

typedef struct {
    uint16_t type;                 /* T_FREE / T_DIR / T_FILE */
    uint16_t mode;                 /* 权限，如 0755 */
    uint16_t uid;                  /* 所有者 */
    uint16_t gid;                  /* 所属组 */
    uint32_t size;                 /* 文件字节数 */
    uint32_t ctime;                /* 创建时间（Unix 时间戳） */
    uint32_t mtime;                /* 修改时间 */
    uint32_t direct[NDIRECT];      /* 直接块号，0 表示未分配 */
    uint32_t indirect;             /* 一级间接块号，0 表示未分配 */
} inode_t;

/* ---------- 目录项：目录文件的内容就是一串目录项 ---------- */
#define DIRENT_SIZE   32
#define NAME_MAX_LEN  27           /* 文件名最多 27 字节（再加 1 字节 '\0'） */

typedef struct {
    uint32_t inum;                 /* 0 表示这个槽位空闲 */
    char     name[NAME_MAX_LEN + 1];
} dirent_t;

/* ---------- 用户表 ---------- */
#define MAX_USERS     16           /* uid 就是用户在表中的下标 */
#define USERNAME_LEN  15
#define PASSWORD_LEN  15

typedef struct {
    uint8_t used;
    uint8_t gid;
    char    name[USERNAME_LEN + 1];
    char    password[PASSWORD_LEN + 1];   /* 明文，仅供演示 */
} user_t;

_Static_assert(sizeof(superblock_t) <= BLOCK_SIZE, "superblock too large");
_Static_assert(sizeof(inode_t) == INODE_SIZE, "INODE_SIZE does not match inode_t");
_Static_assert(BLOCK_SIZE % INODE_SIZE == 0, "inodes must not span blocks");
_Static_assert(sizeof(dirent_t) == DIRENT_SIZE, "DIRENT_SIZE does not match dirent_t");
_Static_assert(sizeof(user_t) * MAX_USERS <= BLOCK_SIZE, "user table must fit in one block");
_Static_assert(NBLOCKS <= BLOCK_SIZE * 8, "bitmap must fit in one block");

/* ---------- 错误码：所有函数失败时返回负数 ---------- */
enum {
    FS_OK      = 0,
    E_NOENT    = -1,    /* 文件或目录不存在 */
    E_EXIST    = -2,    /* 已存在 */
    E_NOTDIR   = -3,    /* 不是目录 */
    E_ISDIR    = -4,    /* 是目录 */
    E_NOTEMPTY = -5,    /* 目录非空 */
    E_NOSPC    = -6,    /* 没有空闲数据块 */
    E_NOINODE  = -7,    /* 没有空闲 inode */
    E_FBIG     = -8,    /* 超过最大文件大小 */
    E_ACCES    = -9,    /* 权限位不允许 */
    E_PERM     = -10,   /* 身份不允许（非所有者、非 root） */
    E_NAME     = -11,   /* 名字或路径不合法 */
    E_INVAL    = -12,   /* 参数不合法 */
    E_BUSY     = -13,   /* 根目录或当前目录不能删除 */
    E_BADFS    = -14,   /* 不是合法的磁盘镜像 */
    E_IO       = -15,   /* 打开/创建镜像失败 */
};

const char *fs_strerror(int err);

#endif
