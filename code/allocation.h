#ifndef _ALLOCATION__H
#define _ALLOCATION__H

#include<stdio.h>
#include<stdlib.h>
#include<malloc.h>
#include<string.h>
#include<time.h>
#include<ctype.h>


#define BLOCKCOUNT 1024
#define BLOCKSIZE 4096  
//4096/sizeof(file_)==14
#define ALLOCATIONSPACESIZE 1024*4
#define SQR_NODECOUNT 32

#define INIT_ALLOCATION memset(allocation, 0, sizeof(allocation));
//初始化allocation分配表的宏
#define INIT_NODESSIGN for(int i=0;i<BLOCKCOUNT;i++){nodes[i].sign=0;}
//初始化每个node的sign标志的宏




/*一个node_实际上就是一个block，即内存块*/
typedef struct {
	int 	sign; 						//node节点类型(0---直接快；1---以及索引块；2---二级索引块；3---三级索引块)
										//由于时间精力原因并未实现多级索引，暂时没用到
	char 	content[4096];
} node_;




int allocation[SQR_NODECOUNT][SQR_NODECOUNT];
/*默认nodes[0]存储根目录、nodesp[1]存储groups[]、nodes[2]存储allocation分配表*/
node_ nodes[BLOCKCOUNT];
int alloc_mark_pos = 3;				//记录上次分配空闲node下标的下一个pos（初始为3）（nodes[0、1、2]是系统node）



//寻找空闲块，返回在nodes[]中的index下标(返回负值代表没找到)
int getFreeNodeIndex() {

	int i = alloc_mark_pos / SQR_NODECOUNT;
	int j = alloc_mark_pos % SQR_NODECOUNT;
	
	for (; i <= SQR_NODECOUNT; i++) {
		for (; j <= SQR_NODECOUNT; j++) {
			if (allocation[i][j] == 0) {
				alloc_mark_pos = i * SQR_NODECOUNT + j + 1;
				return i * SQR_NODECOUNT + j;
			}
		}
	}
	/*如果从上次分配的下一个位置往后找，没找到，有可能alloc_mark_pos前有节点被释放，重新找*/
	if (alloc_mark_pos > 3) {			//（nodes[0、1、2]是系统node）
		alloc_mark_pos = 3;
		return getFreeNodeIndex();
	} else
		return -1;
}


//将下标为freeindexnode分配掉(之所以独立成一个函数是保证操作的原子性)
void allocNode(int *pnodepos,int freeindex){
	*pnodepos = freeindex;
	int i = freeindex/SQR_NODECOUNT;
	int j = freeindex%SQR_NODECOUNT;
	allocation[i][j] = 1;
}

#endif





