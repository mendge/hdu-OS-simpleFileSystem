#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bitmap.h"
#include "dir.h"
#include "disk.h"
#include "inode.h"
#include "shell.h"
#include "user.h"

#define LINE_MAX_LEN   512
#define MAX_ARGS       8
#define TREE_MAX_DEPTH 32

/* ---------------------------------------------------------------- 输入 */

int read_line(char *buf, size_t size)
{
    size_t len;
    int c;

    if (!fgets(buf, (int)size, stdin))
        return READ_EOF;
    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[--len] = '\0';
        if (len > 0 && buf[len - 1] == '\r')
            buf[--len] = '\0';
        return (int)len;
    }
    if (feof(stdin))
        return (int)len;                /* 最后一行没有换行符 */
    while ((c = getchar()) != EOF && c != '\n')
        ;                               /* 丢弃这一行剩下的部分 */
    return READ_TOOLONG;
}

/*
 * 读入多行文本，直到某一行只有一个 "." 或输入结束。
 * 超过最大文件大小时继续读完（免得剩下的内容被当成命令），但标记 too_big。
 */
static char *read_text(uint32_t *len_out, int *too_big)
{
    size_t cap = 4096, len = 0;
    char *text = malloc(cap);
    char chunk[1024];
    int line_start = 1;

    if (!text)
        panic("out of memory");
    *too_big = 0;
    while (fgets(chunk, sizeof chunk, stdin)) {
        size_t n = strlen(chunk);

        if (line_start && (strcmp(chunk, ".\n") == 0 || strcmp(chunk, ".\r\n") == 0 || strcmp(chunk, ".") == 0))
            break;
        line_start = chunk[n - 1] == '\n';
        if (len + n > MAX_FILE_SIZE) {
            *too_big = 1;
            continue;
        }
        if (len + n > cap) {
            while (len + n > cap)
                cap *= 2;
            if (!(text = realloc(text, cap)))
                panic("out of memory");
        }
        memcpy(text + len, chunk, n);
        len += n;
    }
    *len_out = (uint32_t)len;
    return text;
}

/* ---------------------------------------------------------------- 输出辅助 */

static void mode_string(const inode_t *ino, char out[11])
{
    const char *rwx = "rwxrwxrwx";

    out[0] = ino->type == T_DIR ? 'd' : '-';
    for (int i = 0; i < 9; i++)
        out[i + 1] = (ino->mode >> (8 - i)) & 1 ? rwx[i] : '-';
    out[10] = '\0';
}

static void format_time(uint32_t t, char *buf, size_t size)
{
    time_t tt = (time_t)t;

    strftime(buf, size, "%Y-%m-%d %H:%M", localtime(&tt));
}

static void print_entry(const char *name, const inode_t *ino)
{
    char mode[11], owner[USERNAME_LEN + 1], when[32];

    mode_string(ino, mode);
    user_name(ino->uid, owner);
    format_time(ino->mtime, when, sizeof when);
    printf("%s %-8s %-6s %7u %s %s%s\n", mode, owner, group_name(ino->gid),
           (unsigned)ino->size, when, name, ino->type == T_DIR ? "/" : "");
}

static int is_child(const dirent_t *de)
{
    return de->inum && strcmp(de->name, ".") != 0 && strcmp(de->name, "..") != 0;
}

/* ---------------------------------------------------------------- 命令 */

static int cmd_help(session_t *s, int argc, char **argv);

static int cmd_ls(session_t *s, int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : ".";
    uint32_t inum;
    inode_t ino;
    dirent_t de;
    int r;

    if ((r = fs_lookup(s, path, &inum)) != FS_OK)
        return r;
    iread(inum, &ino);
    if (ino.type != T_DIR) {
        print_entry(path, &ino);
        return FS_OK;
    }
    if (!fs_permitted(s, &ino, PERM_R))
        return E_ACCES;
    for (uint32_t idx = 0; dir_read_entry(inum, idx, &de); idx++) {
        inode_t child;

        if (!is_child(&de))
            continue;
        iread(de.inum, &child);
        print_entry(de.name, &child);
    }
    return FS_OK;
}

static int cmd_cd(session_t *s, int argc, char **argv)
{
    return fs_chdir(s, argc > 1 ? argv[1] : "/");
}

static int cmd_pwd(session_t *s, int argc, char **argv)
{
    char cwd[PATH_MAX_LEN + 1];
    int r = fs_getcwd(s, cwd, sizeof cwd);

    (void)argc;
    (void)argv;
    if (r == FS_OK)
        puts(cwd);
    return r;
}

static int cmd_mkdir(session_t *s, int argc, char **argv)
{
    (void)argc;
    return fs_mkdir(s, argv[1]);
}

static int cmd_touch(session_t *s, int argc, char **argv)
{
    (void)argc;
    return fs_touch(s, argv[1]);
}

static int do_write(session_t *s, const char *path, int append)
{
    uint32_t inum, len;
    inode_t ino;
    int too_big, r;
    char *text;

    /* 先检查目标，免得用户输入完一大段文字才发现写不了 */
    r = fs_open(s, path, PERM_W, &inum, &ino);
    if (r == FS_OK && ino.type == T_DIR)
        return E_ISDIR;
    if (r != FS_OK && r != E_NOENT)
        return r;

    printf("(enter text, finish with a line containing only '.')\n");
    text = read_text(&len, &too_big);
    r = too_big ? E_FBIG : fs_write(s, path, text, len, append);
    free(text);
    if (r == FS_OK)
        printf("%u bytes written\n", (unsigned)len);
    return r;
}

static int cmd_write(session_t *s, int argc, char **argv)
{
    (void)argc;
    return do_write(s, argv[1], 0);
}

static int cmd_append(session_t *s, int argc, char **argv)
{
    (void)argc;
    return do_write(s, argv[1], 1);
}

static int cmd_cat(session_t *s, int argc, char **argv)
{
    uint32_t inum, off = 0;
    inode_t ino;
    char buf[BLOCK_SIZE];
    int n, r;

    (void)argc;
    if ((r = fs_open(s, argv[1], PERM_R, &inum, &ino)) != FS_OK)
        return r;
    if (ino.type == T_DIR)
        return E_ISDIR;
    while ((n = readi(inum, buf, off, sizeof buf)) > 0) {
        fwrite(buf, 1, (size_t)n, stdout);
        off += (uint32_t)n;
    }
    if (off > 0 && buf[(off - 1) % sizeof buf] != '\n')
        putchar('\n');                  /* 文件末尾没有换行时补一个，让提示符另起一行 */
    return FS_OK;
}

static int cmd_rm(session_t *s, int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "-r") == 0)
        return fs_remove(s, argv[2], 1);
    if (argc == 2 && argv[1][0] != '-')
        return fs_remove(s, argv[1], 0);
    return E_INVAL;
}

static int cmd_chmod(session_t *s, int argc, char **argv)
{
    char *end;
    long mode = strtol(argv[1], &end, 8);

    (void)argc;
    if (*argv[1] == '\0' || *end != '\0' || mode < 0 || mode > 0777)
        return E_INVAL;
    return fs_chmod(s, argv[2], (uint16_t)mode);
}

static int cmd_stat(session_t *s, int argc, char **argv)
{
    uint32_t inum;
    inode_t ino;
    char mode[11], owner[USERNAME_LEN + 1], when[32];
    int r;

    (void)argc;
    if ((r = fs_lookup(s, argv[1], &inum)) != FS_OK)
        return r;
    iread(inum, &ino);
    mode_string(&ino, mode);
    user_name(ino.uid, owner);

    printf("  File: %s\n", argv[1]);
    printf(" Inode: %u    Type: %s\n", (unsigned)inum, ino.type == T_DIR ? "directory" : "file");
    printf("  Mode: %04o (%s)    Owner: %s (uid %u)    Group: %s (gid %u)\n",
           (unsigned)ino.mode, mode, owner, (unsigned)ino.uid, group_name(ino.gid), (unsigned)ino.gid);
    printf("  Size: %u bytes    Data blocks: %u\n", (unsigned)ino.size, (unsigned)inode_count_blocks(&ino));
    printf("Direct:");
    for (int i = 0; i < NDIRECT; i++) {
        if (ino.direct[i])
            printf(" %u", (unsigned)ino.direct[i]);
        else
            printf(" -");
    }
    printf("\n");
    if (ino.indirect) {
        uint32_t table[NINDIRECT], used = 0;

        disk_read(ino.indirect, table);
        for (uint32_t i = 0; i < NINDIRECT; i++)
            used += table[i] != 0;
        printf("Indirect: block %u (%u of %u entries used)\n", (unsigned)ino.indirect,
               (unsigned)used, (unsigned)NINDIRECT);
    } else {
        printf("Indirect: -\n");
    }
    format_time(ino.ctime, when, sizeof when);
    printf("Create: %s\n", when);
    format_time(ino.mtime, when, sizeof when);
    printf("Modify: %s\n", when);
    return FS_OK;
}

static void tree_walk(const session_t *s, uint32_t dir, const char *prefix, int depth)
{
    inode_t dino;
    dirent_t de;
    uint32_t total = 0, seen = 0;

    iread(dir, &dino);
    if (!fs_permitted(s, &dino, PERM_R | PERM_X)) {
        printf("%s`-- [permission denied]\n", prefix);
        return;
    }
    for (uint32_t idx = 0; dir_read_entry(dir, idx, &de); idx++)
        total += is_child(&de);

    for (uint32_t idx = 0; dir_read_entry(dir, idx, &de); idx++) {
        inode_t ino;
        int last;

        if (!is_child(&de))
            continue;
        last = ++seen == total;
        iread(de.inum, &ino);
        printf("%s%s%s%s\n", prefix, last ? "`-- " : "|-- ", de.name, ino.type == T_DIR ? "/" : "");
        if (ino.type == T_DIR && depth < TREE_MAX_DEPTH) {
            char next[4 * TREE_MAX_DEPTH + 8];

            snprintf(next, sizeof next, "%s%s", prefix, last ? "    " : "|   ");
            tree_walk(s, de.inum, next, depth + 1);
        }
    }
}

static int cmd_tree(session_t *s, int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : ".";
    uint32_t inum;
    inode_t ino;
    int r;

    if ((r = fs_lookup(s, path, &inum)) != FS_OK)
        return r;
    iread(inum, &ino);
    if (ino.type != T_DIR)
        return E_NOTDIR;
    printf("%s\n", path);
    tree_walk(s, inum, "", 1);
    return FS_OK;
}

static int cmd_df(session_t *s, int argc, char **argv)
{
    uint32_t data_blocks = NBLOCKS - DATA_START;
    uint32_t free_blocks = bitmap_count_free();
    uint32_t free_inodes = inode_count_free();

    (void)s;
    (void)argc;
    (void)argv;
    printf("blocks: %u data blocks, %u used, %u free (block size %u bytes)\n",
           (unsigned)data_blocks, (unsigned)(data_blocks - free_blocks), (unsigned)free_blocks,
           (unsigned)BLOCK_SIZE);
    printf("inodes: %u total, %u used, %u free\n",
           (unsigned)(NINODES - 1), (unsigned)(NINODES - 1 - free_inodes), (unsigned)free_inodes);
    return FS_OK;
}

static int cmd_whoami(session_t *s, int argc, char **argv)
{
    char name[USERNAME_LEN + 1];

    (void)argc;
    (void)argv;
    user_name(s->uid, name);
    printf("%s (uid %u, group %s)\n", name, (unsigned)s->uid, group_name(s->gid));
    return FS_OK;
}

static int cmd_users(session_t *s, int argc, char **argv)
{
    (void)s;
    (void)argc;
    (void)argv;
    printf("uid  name             group\n");
    for (uint16_t uid = 0; uid < MAX_USERS; uid++) {
        user_t u;

        if (user_get(uid, &u) == FS_OK)
            printf("%-4u %-16s %s\n", (unsigned)uid, u.name, group_name(u.gid));
    }
    return FS_OK;
}

static int cmd_useradd(session_t *s, int argc, char **argv)
{
    char password[LINE_MAX_LEN];
    int gid = 1, uid, n;

    if (s->uid != ROOT_UID)
        return E_PERM;
    if (argc > 2 && (gid = group_find(argv[2])) < 0)
        return E_INVAL;
    if (user_find(argv[1]) >= 0)
        return E_EXIST;                 /* 先查重名，免得白输一次密码 */
    printf("password: ");
    fflush(stdout);
    n = read_line(password, sizeof password);
    if (n < 0)
        return E_INVAL;
    if ((uid = user_add(argv[1], password, (uint16_t)gid)) < 0)
        return uid;
    printf("user %s created (uid %d, group %s)\n", argv[1], uid, group_name((uint16_t)gid));
    return FS_OK;
}

static int cmd_userdel(session_t *s, int argc, char **argv)
{
    int uid;

    (void)argc;
    if (s->uid != ROOT_UID)
        return E_PERM;
    if ((uid = user_del(argv[1])) < 0)
        return uid;
    fs_reassign_owner((uint16_t)uid, ROOT_UID, 0);  /* 被删用户的文件转给 root */
    printf("user %s deleted, its files now belong to root\n", argv[1]);
    return FS_OK;
}

static int cmd_logout(session_t *s, int argc, char **argv)
{
    (void)s;
    (void)argc;
    (void)argv;
    return SHELL_LOGOUT;
}

static int cmd_exit(session_t *s, int argc, char **argv)
{
    (void)s;
    (void)argc;
    (void)argv;
    return SHELL_EXIT;
}

typedef int (*cmd_fn)(session_t *s, int argc, char **argv);

static const struct command {
    const char *name;
    int         min_args, max_args;     /* 不含命令名本身 */
    cmd_fn      fn;
    const char *usage;
    const char *help;
} commands[] = {
    { "help",    0, 0, cmd_help,    "help",                "show this help" },
    { "ls",      0, 1, cmd_ls,      "ls [path]",           "list a directory" },
    { "cd",      0, 1, cmd_cd,      "cd [path]",           "change directory (default /)" },
    { "pwd",     0, 0, cmd_pwd,     "pwd",                 "print working directory" },
    { "mkdir",   1, 1, cmd_mkdir,   "mkdir <path>",        "create a directory" },
    { "touch",   1, 1, cmd_touch,   "touch <path>",        "create an empty file" },
    { "write",   1, 1, cmd_write,   "write <path>",        "overwrite a file with typed text" },
    { "append",  1, 1, cmd_append,  "append <path>",       "append typed text to a file" },
    { "cat",     1, 1, cmd_cat,     "cat <path>",          "print a file" },
    { "rm",      1, 2, cmd_rm,      "rm [-r] <path>",      "remove a file or directory" },
    { "chmod",   2, 2, cmd_chmod,   "chmod <mode> <path>", "change permissions, e.g. chmod 640 a.txt" },
    { "stat",    1, 1, cmd_stat,    "stat <path>",         "show inode details and block numbers" },
    { "tree",    0, 1, cmd_tree,    "tree [path]",         "show a directory tree" },
    { "df",      0, 0, cmd_df,      "df",                  "show free blocks and inodes" },
    { "whoami",  0, 0, cmd_whoami,  "whoami",              "show current user" },
    { "users",   0, 0, cmd_users,   "users",               "list users" },
    { "useradd", 1, 2, cmd_useradd, "useradd <name> [root|staff|guest]", "add a user (root only)" },
    { "userdel", 1, 1, cmd_userdel, "userdel <name>",      "delete a user (root only)" },
    { "logout",  0, 0, cmd_logout,  "logout",              "log out" },
    { "exit",    0, 0, cmd_exit,    "exit",                "quit the program" },
};

#define NCOMMANDS (sizeof commands / sizeof commands[0])

static int cmd_help(session_t *s, int argc, char **argv)
{
    (void)s;
    (void)argc;
    (void)argv;
    for (size_t i = 0; i < NCOMMANDS; i++)
        printf("  %-36s %s\n", commands[i].usage, commands[i].help);
    return FS_OK;
}

/* ---------------------------------------------------------------- 主循环 */

int shell_login(session_t *s)
{
    char name[LINE_MAX_LEN], password[LINE_MAX_LEN];
    uint16_t uid, gid;

    for (;;) {
        printf("login: ");
        fflush(stdout);
        if (read_line(name, sizeof name) == READ_EOF)
            return READ_EOF;
        if (name[0] == '\0')
            continue;
        printf("password: ");
        fflush(stdout);
        if (read_line(password, sizeof password) == READ_EOF)
            return READ_EOF;
        if (user_auth(name, password, &uid, &gid) == FS_OK) {
            s->uid = uid;
            s->gid = gid;
            s->cwd = ROOT_INUM;
            printf("welcome, %s. type 'help' for commands.\n", name);
            return 0;
        }
        printf("login incorrect\n");
    }
}

static void report(const char *cmd, int err)
{
    fflush(stdout);
    fprintf(stderr, "%s: %s\n", cmd, fs_strerror(err));
}

int shell_run(session_t *s)
{
    char line[LINE_MAX_LEN];

    for (;;) {
        char cwd[PATH_MAX_LEN + 1], name[USERNAME_LEN + 1];
        char *argv[MAX_ARGS + 1];
        const struct command *cmd = NULL;
        int argc = 0, n, r;

        if (fs_getcwd(s, cwd, sizeof cwd) != FS_OK)
            strcpy(cwd, "?");
        user_name(s->uid, name);
        printf("[%s@fs %s]%c ", name, cwd, s->uid == ROOT_UID ? '#' : '$');
        fflush(stdout);

        n = read_line(line, sizeof line);
        if (n == READ_EOF) {
            printf("\n");
            return SHELL_EXIT;          /* 输入结束（Ctrl-D 或脚本读完）就正常退出 */
        }
        if (n == READ_TOOLONG) {
            report("shell", E_INVAL);
            continue;
        }

        for (char *tok = strtok(line, " \t"); tok; tok = strtok(NULL, " \t")) {
            if (argc == MAX_ARGS + 1)
                break;
            argv[argc++] = tok;
        }
        if (argc == 0)
            continue;

        for (size_t i = 0; i < NCOMMANDS; i++)
            if (strcmp(argv[0], commands[i].name) == 0)
                cmd = &commands[i];
        if (!cmd) {
            fflush(stdout);
            fprintf(stderr, "%s: command not found (type 'help')\n", argv[0]);
            continue;
        }
        if (argc - 1 < cmd->min_args || argc - 1 > cmd->max_args) {
            fflush(stdout);
            fprintf(stderr, "usage: %s\n", cmd->usage);
            continue;
        }

        r = cmd->fn(s, argc, argv);
        if (r == SHELL_LOGOUT || r == SHELL_EXIT)
            return r;
        if (r == E_INVAL && strcmp(cmd->name, "rm") == 0) {
            fflush(stdout);
            fprintf(stderr, "usage: %s\n", cmd->usage);
        } else if (r < 0) {
            report(cmd->name, r);
        }
    }
}
