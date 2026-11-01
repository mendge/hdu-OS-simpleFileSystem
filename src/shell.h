/*
 * shell.h —— 模拟的命令行：登录、解析命令、调用 fsops 并打印结果
 */
#ifndef SHELL_H
#define SHELL_H

#include <stddef.h>

#include "fsops.h"

enum { SHELL_LOGOUT = 1, SHELL_EXIT = 2 };
enum { READ_EOF = -1, READ_TOOLONG = -2 };

/* 读一行（去掉换行符），返回长度；遇到 EOF 返回 READ_EOF，行太长返回 READ_TOOLONG */
int read_line(char *buf, size_t size);

/* 登录成功返回 0 并填好会话；输入结束返回 READ_EOF */
int shell_login(session_t *s);

/* 运行命令循环，返回 SHELL_LOGOUT 或 SHELL_EXIT */
int shell_run(session_t *s);

#endif
