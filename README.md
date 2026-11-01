# 简单文件系统（HDU 操作系统课程设计）

这是一个用 C 语言写的教学用文件系统。它把一个 1 MiB 的普通文件（磁盘镜像 `disk.img`）当作磁盘，在上面实现了超级块、位示图、inode、多级目录、混合索引、多用户和权限控制，并提供一个类 Unix 的命令行来操作它。

代码约 2000 行（含注释），分层清晰，每一层只依赖它下面的层，适合边读代码边理解"文件系统是怎么把字节组织成文件和目录的"。

有问题欢迎提 issue，或发邮件到 **zzkzs01@163.com**。

## 快速开始

需要一个 C11 编译器和 `make`（macOS、Linux、Windows 的 MinGW/MSYS2 都可以）。

```bash
make
```

```bash
./build/fs
```

第一次运行时会自动创建并格式化 `disk.img`。默认账号是 `root`，密码是 `123456`。镜像也可以换成别的文件名：`./build/fs my.img`。加上 `-f` 会重新格式化镜像，里面原有的内容会全部清空。

所有修改都会**立即写入镜像**，下次启动时依然存在。即使程序中途被强制结束，已经完成的操作也不会丢失。

运行回归测试：

```bash
make test
```

## 一次示例会话

```text
login: root
password: 123456
welcome, root. type 'help' for commands.
[root@fs /]# mkdir /docs
[root@fs /]# cd /docs
[root@fs /docs]# write hello.txt
(enter text, finish with a line containing only '.')
Hello, file system!
.
20 bytes written
[root@fs /docs]# ls
-rw-r--r-- root     root        20 2026-10-31 19:14 hello.txt
[root@fs /docs]# stat hello.txt
  File: hello.txt
 Inode: 3    Type: file
  Mode: 0644 (-rw-r--r--)    Owner: root (uid 0)    Group: root (gid 0)
  Size: 20 bytes    Data blocks: 1
Direct: 13 - - - - - - - - -
Indirect: -
...
[root@fs /docs]# cd /
[root@fs /]# tree
.
`-- docs/
    `-- hello.txt
[root@fs /]# df
blocks: 1013 data blocks, 3 used, 1010 free (block size 1024 bytes)
inodes: 127 total, 3 used, 124 free
```

`stat` 会显示文件占用了哪些磁盘块，`df` 显示还剩多少空闲块和空闲 inode。课堂演示时，可以在创建和删除文件前后各看一眼，观察分配与回收的过程。

## 命令一览

所有路径都可以用绝对路径（`/a/b`），也可以用相对路径（`b`、`../c`），路径中可以包含 `.` 和 `..`。

| 命令 | 说明 |
| --- | --- |
| `ls [path]` | 列出目录内容（需要该目录的 r 权限） |
| `cd [path]` | 切换当前目录，不带参数回到 `/` |
| `pwd` | 显示当前目录 |
| `mkdir <path>` | 创建目录 |
| `touch <path>` | 创建空文件；文件已存在时只更新修改时间 |
| `write <path>` | 覆盖写：逐行输入文本，单独一行 `.` 表示结束；文件不存在时自动创建 |
| `append <path>` | 追加写，输入方式同上 |
| `cat <path>` | 显示文件内容 |
| `rm [-r] <path>` | 删除文件或空目录；加 `-r` 递归删除非空目录 |
| `chmod <mode> <path>` | 修改权限（八进制），例如 `chmod 640 a.txt`；只有所有者或 root 可以修改 |
| `stat <path>` | 显示 inode 详情和占用的块号 |
| `tree [path]` | 以树形显示目录结构 |
| `df` | 显示空闲块和空闲 inode 数量 |
| `whoami` / `users` | 显示当前用户 / 所有用户 |
| `useradd <name> [root\|staff\|guest]` | 添加用户，默认加入 staff 组（仅 root 可用） |
| `userdel <name>` | 删除用户，其名下文件转给 root（仅 root 可用） |
| `logout` / `exit` | 注销回到登录界面 / 退出程序（直接按 Ctrl-D 也会退出） |
| `help` | 显示帮助 |

## 设计概览

磁盘镜像的布局：

```text
块号:   0        1        2        3 ... 10        11 ... 1023
     +--------+--------+--------+-------------+-----------------+
     | 超级块  | 位示图  | 用户表  |  inode 表   |     数据块       |
     +--------+--------+--------+-------------+-----------------+
```

- **inode**：每个文件和目录对应一个 64 字节的 inode，记录类型、权限、所有者、大小、时间，以及 10 个直接块号和 1 个一级间接块号。单个文件最大为 (10 + 256) × 1 KiB = 266 KiB。
- **目录**：目录本身也是文件，内容是一串 32 字节的目录项，每项是"名字 → inode 号"。每个目录都包含 `.` 和 `..` 两项。
- **位示图**：用 1 位记录 1 个块是否被占用。
- **权限**：采用和 Unix 相同的 `rwx` 三组权限位（所有者 / 同组用户 / 其他人），root 不受权限限制。

源码按层组织，上层只调用下层：

| 层 | 文件 | 职责 |
| --- | --- | --- |
| 命令行 | `src/shell.c`、`src/main.c` | 登录、解析命令、打印结果 |
| 文件操作 | `src/fsops.c` | 格式化与挂载、路径解析、权限检查、mkdir/write/rm 等 |
| 目录 | `src/dir.c` | 在目录文件中查找、添加、删除目录项 |
| 用户 | `src/user.c` | 用户表与组 |
| inode | `src/inode.c` | 分配和释放 inode；混合索引；按字节偏移读写文件内容 |
| 位示图 | `src/bitmap.c` | 分配和回收数据块 |
| 磁盘 | `src/disk.c` | 以块为单位读写镜像文件 |
| 布局 | `src/fs.h` | 常量、磁盘上的结构体、错误码 |

更完整的设计说明见 [docs/design.md](docs/design.md)。

## 继续改进

[docs/exercises.md](docs/exercises.md) 整理了一组由易到难的扩展练习，例如二级间接索引、`mv`/重命名、硬链接、文件打开表、块缓存、崩溃一致性等。