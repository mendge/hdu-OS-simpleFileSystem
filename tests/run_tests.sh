#!/bin/sh
# 回归测试：用脚本化的输入驱动 shell，检查输出中是否包含预期内容。
# 用法：make test   或   BIN=build/fs sh tests/run_tests.sh

set -u
BIN=${BIN:-build/fs}
TMP=$(mktemp -d "${TMPDIR:-/tmp}/fs-test.XXXXXX")
trap 'rm -rf "$TMP"' EXIT
PASS=0
FAIL=0
CASE=""

# run_fs [args...]：以 $TMP/in 为标准输入运行程序，输出写入 $TMP/out。
# 10 秒超时 + 输出大小上限，防止程序死循环时拖垮机器。
run_fs() {
    ( ulimit -f 20480; exec "$BIN" "$@" ) < "$TMP/in" > "$TMP/out" 2>&1 &
    pid=$!
    ( sleep 10; kill "$pid" 2>/dev/null ) &
    watchdog=$!
    wait "$pid"
    STATUS=$?
    kill "$watchdog" 2>/dev/null
    wait "$watchdog" 2>/dev/null
}

begin() { CASE=$1; }
ok()    { PASS=$((PASS + 1)); }
bad()   { FAIL=$((FAIL + 1)); echo "FAIL [$CASE] $1"; }

expect()      { grep -qF -- "$1" "$TMP/out" && ok || bad "missing: $1"; }
# expect_end：某一行以给定内容结尾（没有输出的命令会让多个提示符挤在同一行）
expect_end()  { awk -v s="$1" 'substr($0, length($0) - length(s) + 1) == s { f = 1 } END { exit !f }' "$TMP/out" && ok || bad "no line ending with: $1"; }
reject()      { grep -qF -- "$1" "$TMP/out" && bad "unexpected: $1" || ok; }
expect_status() { [ "$STATUS" -eq "$1" ] && ok || bad "exit status $STATUS, expected $1"; }

# gen_lines N：生成 N 行、每行 43 字节的文本
gen_lines() { awk -v n="$1" 'BEGIN { for (i = 1; i <= n; i++) printf "line %06d ..............................\n", i }'; }

# ------------------------------------------------------------------ 登录
begin login
cat > "$TMP/in" <<'EOF'
nobody
x
root
wrong
root
123456
whoami
exit
EOF
run_fs "$TMP/login.img"
expect "formatting new disk image"
expect "login incorrect"
expect "root (uid 0, group root)"
expect_status 0

# ------------------------------------------------------------------ 目录与路径
begin paths
cat > "$TMP/in" <<'EOF'
root
123456
mkdir /a
mkdir a/b
cd a/b
mkdir c
cd c
pwd
cd ../../..
pwd
cd /a/b/c/../..
pwd
mkdir /a
cd nope
mkdir /nope/x
mkdir abcdefghijklmnopqrstuvwxyz0123
mkdir /a/..
ls /a
EOF
run_fs -f "$TMP/paths.img"
expect_end "[root@fs /a/b/c]# /a/b/c"
expect_end "[root@fs /]# /"
expect_end "[root@fs /a]# /a"
expect "mkdir: file exists"
expect "cd: no such file or directory"
expect "mkdir: no such file or directory"
expect "mkdir: invalid name"
expect " b/"

# ------------------------------------------------------------------ 读写
begin readwrite
cat > "$TMP/in" <<'EOF'
root
123456
write /f.txt
hello world, GOOD day
EOF letters: E O F
.
cat /f.txt
append /f.txt
second
.
cat f.txt
write f.txt
short
.
cat f.txt
cd f.txt
cat /
write /
mkdir /f.txt/x
exit
EOF
run_fs -f "$TMP/rw.img"
expect "hello world, GOOD day"
expect "EOF letters: E O F"
expect "second"
expect_end "[root@fs /]# short"
expect "cd: not a directory"
expect "cat: is a directory"
expect "write: is a directory"
expect "mkdir: not a directory"

# ------------------------------------------------------------------ 大文件、间接块、空间回收
begin bigfile
gen_lines 600 > "$TMP/big.txt"          # 25800 字节 > 10 个直接块，会用到间接块
SIZE=$(wc -c < "$TMP/big.txt" | tr -d ' ')
{
    printf 'root\n123456\ndf\nwrite /big\n'
    cat "$TMP/big.txt"
    printf '.\nstat /big\ncat /big\nrm /big\ndf\nexit\n'
} > "$TMP/in"
run_fs -f "$TMP/big.img"
expect "Size: $SIZE bytes"
expect "Indirect: block"
expect "line 000600"
DF=$(grep -o "data blocks, .*" "$TMP/out" | sort -u | wc -l | tr -d ' ')
[ "$DF" -eq 1 ] && ok || bad "df differs after rm (block leak)"

begin toolarge
{
    printf 'root\n123456\nwrite /huge\n'
    gen_lines 7000                       # 301000 字节 > 266 KiB 上限
    printf '.\nstat /huge\nexit\n'
} > "$TMP/in"
run_fs -f "$TMP/huge.img"
expect "write: file too large"
expect "stat: no such file or directory"

begin diskfull
{
    printf 'root\n123456\ndf\n'
    for f in 1 2 3 4; do
        printf 'write /f%s\n' "$f"
        gen_lines 6200                   # 每个约 260 KiB，4 个装不下 1 MiB 的盘
        printf '.\n'
    done
    printf 'rm /f1\nrm /f2\nrm /f3\nrm /f4\ndf\nexit\n'
} > "$TMP/in"
run_fs -f "$TMP/full.img"
expect "write: no space left on device"
DF=$(grep -o "data blocks, .*" "$TMP/out" | sort -u | wc -l | tr -d ' ')
[ "$DF" -eq 1 ] && ok || bad "df differs after filling and emptying the disk"

# ------------------------------------------------------------------ 删除
begin remove
cat > "$TMP/in" <<'EOF'
root
123456
mkdir /d
mkdir /d/e
touch /d/e/x
rm /d
rm -r /d
stat /d
mkdir /p
cd /p
rm -r /p
rm .
rm /
rm -x /p
cd /
rm -r /p
stat /p
exit
EOF
run_fs -f "$TMP/rm.img"
expect "rm: directory not empty"
expect "stat: no such file or directory"
expect "rm: resource busy"
expect "usage: rm [-r] <path>"

# ------------------------------------------------------------------ 持久化与 EOF
begin persist
cat > "$TMP/in" <<'EOF'
root
123456
mkdir /keep
write /keep/n
persist me
.
exit
EOF
run_fs -f "$TMP/p.img"
cat > "$TMP/in" <<'EOF'
root
123456
cat /keep/n
EOF
run_fs "$TMP/p.img"                      # 第二次运行不带 -f，并且没有 exit 直接 EOF
expect "persist me"
reject "formatting"
expect_status 0

# ------------------------------------------------------------------ 用户与权限
begin permissions
cat > "$TMP/in" <<'EOF'
root
123456
useradd alice staff
alicepw
useradd alice
mkdir /tmp
chmod 777 /tmp
write /secret
top secret
.
chmod 600 /secret
logout
alice
alicepw
whoami
cat /secret
write /secret
mkdir /nope
write /tmp/mine
mine
.
cat /tmp/mine
chmod 600 /secret
useradd bob
userdel root
logout
root
123456
cat /secret
userdel alice
stat /tmp/mine
useradd root
userdel root
users
exit
EOF
run_fs -f "$TMP/perm.img"
expect "user alice created (uid 1, group staff)"
expect "useradd: file exists"
expect "alice (uid 1, group staff)"
expect "cat: permission denied"
expect "write: permission denied"
expect "mkdir: permission denied"
expect_end "[alice@fs /]$ mine"
expect "chmod: operation not permitted"
expect "useradd: operation not permitted"
expect "userdel: operation not permitted"
expect_end "[root@fs /]# top secret"
expect "Owner: root (uid 0)"

# ------------------------------------------------------------------ 非法镜像
begin badimage
echo "this is not a disk image" > "$TMP/bad.img"
: > "$TMP/in"
run_fs "$TMP/bad.img"
expect "not a valid disk image"
expect_status 1

# ------------------------------------------------------------------
echo "passed: $PASS, failed: $FAIL"
[ "$FAIL" -eq 0 ]
