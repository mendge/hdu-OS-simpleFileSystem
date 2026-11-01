/*
 * 简单文件系统 —— 入口
 *
 * 用法: fs [-f] [image]
 *   image  磁盘镜像文件，默认 disk.img；不存在时自动格式化
 *   -f     强制重新格式化（会清空镜像）
 */
#include <stdio.h>
#include <string.h>

#include "fsops.h"
#include "shell.h"

static void usage(const char *prog)
{
    fprintf(stderr, "usage: %s [-f] [image]\n", prog);
}

int main(int argc, char **argv)
{
    const char *image = "disk.img";
    int format = 0, r;
    FILE *probe;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--format") == 0) {
            format = 1;
        } else if (argv[i][0] == '-') {
            usage(argv[0]);
            return 2;
        } else {
            image = argv[i];
        }
    }

    if ((probe = fopen(image, "rb")) != NULL)
        fclose(probe);
    if (format || probe == NULL) {
        printf("formatting new disk image '%s' (default account: root / 123456)\n", image);
        r = fs_format(image);
    } else {
        r = fs_mount(image);
    }
    if (r != FS_OK) {
        fprintf(stderr, "%s: %s\n", image, fs_strerror(r));
        if (r == E_BADFS)
            fprintf(stderr, "hint: run with -f to format it (this erases the file)\n");
        return 1;
    }

    for (;;) {
        session_t s;

        if (shell_login(&s) == READ_EOF || shell_run(&s) == SHELL_EXIT)
            break;
    }
    fs_unmount();
    return 0;
}
