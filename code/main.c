#include"dir.h"

int main(){	

	load();		//自己初始化文件系统则替换load()为systemInit();
	workspace_ cws;
	initWrokspace(&cws);
	systemOprate(&cws);
	save();
	return 0;
}
