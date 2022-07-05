## main.c

文件系统的运行源代码

## user.h

对用户的操作函数、全局变量以及一些相关的宏定义

## allocation.h

文件系统物理块的分配的函数、全局变量及一些相关宏

## file.h

对文件操作的函数、全局变量以及相关宏



## 头文件包含关系

main.c  include  file.h

file.h  include  user.h  and  allocation.h