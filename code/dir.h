#include<stdio.h>
#include<stdlib.h>
#include<malloc.h>
#include<string.h>
#include<time.h>
#include<ctype.h>
#include"allocation.h"
#include"user.h"

#define NAMESIZE 64						//文件名最大字节数
#define PATHSIZE 128					//路径最大字节数
#define FILESIZE 280					//一个file_结构体的字节数
#define FILECOUNT 14					//一个目录块最多能存储的file_数量（4096/sizeof(file_)==14）

/*用于textfile的混合索引实现*/
#define DLEVELCOUNT 10					//直接块个数
#define FLEVELCOUNT 2					//一级索引块数
#define SLEVELCOUNT 1					//二级索引块数
#define TLEVELCOUNT 1					//三级索引块数
#define INDEXESCOUNT 1024 				//索引块中可存储的索引数（块大小/int型索引项大小==4096/sizeof(int)==1024）

/*逻辑转换宏，数字正负转逻辑0和1*/
#define SUCCESS(figure) (figure>=0)		//要用括号，不然取非就寄了

/*sizeof(file_)=288*/
typedef struct {
	int 	type;						//文件类型：0---dir；1---textfile
	char	name[NAMESIZE];	
	char 	cltauth[5];					//控制权限（control authority）（不同的位对应不同组的用户）
	int		nodealloc[FILECOUNT];		//标记pnode指向node中14个子文件空间的分配情况	
	int 	userpos;					//所属用户
	int 	time;
	char 	fadir[PATHSIZE];			//父目录的完整路径
	int		size;						//文件大小	
	int 	wflag;						//是否有用户再写该文件（互斥写标志）
	int  	nodepos;					//文件对应的nodes存储位置
}file_;


/*工作空间（一个用户操作文件系统时用一个，方便对多用户的实现）*/
typedef struct {
	int 	userpos;//
	file_ 	dirfile;
} workspace_;



/*根目录的创建*/
void createRootdir();

/*初始化位视图和存储节点空间，再初始化根目录*/
void firstInit();

/*获取的是年月日时间，int表示*/ 
int gettime();

/*初始化传入的当前工作空间cws*/
void initWrokspace(workspace_ *cws);

/*再模拟的shell命令行中打印当前用户和当前工作目录文件名*/
void print(workspace_ *cws);

/*将win文件存储的文件系统数据加载进内存正在运行的nodes[]中*/
void load();

/*将内存正在运行的nodes[]存储的文件系统数据保存到win文件进中*/
void save();

/*判断用户对文件是否有读权限*/
int hasReadAutho(char *clkautho, int fileuserpos, int workuserpos);

/*判断用户对文件是否有写权限*/
int hasWriteAutho(char *clkautho, int fileuserpos, int workuserpos) ;

/*判断当前用户是否能共享（ls能打印看到）该文件*/
int getShareAutho(char *clkautho, int fileuserpos, int workuserpos);

/*通过名字，在父目录块中找到子文件file_赋值给传入的subfile指针
	返回值是要找子文件在父目录的索引下标，返回值为负表示没找到*/ 
int getFileByName(char *filename,file_ *fafile, file_ *destfile);

/*获取目录文件未分配目录项索引（非负有效）*/ 
int getFileFreeAllocIndex(int *alloc);

/*通过绝对的文件路径找到file_并赋值给传入指针destfile(返回负值代表没找到)*/
int getFileByAbsPath(char *abspath, file_ *destfile);

/*相关文件同步创建文件带来的父目录分配表更改（祖父目录、父目录、兄弟目录：这些目录都存有对应父目录的file_）*/
int syncFileModification(file_ *fafile);

/*在父目录(当前工作目录)下创建文件*/
int createFile(workspace_ *cws, char *newfilename, int newfiletype);

/*读块封装*/
void readBlock(int nodepos, char *buf, int readsize);

/*写块封装*/
void writeBlock(int nodepos, char *w_content, int writesize);

/*写指定内容w_content进指定pfile(cws的工作目录还是文本文件pfile的父目录)*/
int writeFile(file_ *pfile, char *w_content,workspace_ *cws);

/*将文本文件pfile的内容打印到屏幕*/
void readFile(file_ *pfile,int workuserpos);

/*删除文本文件*/
void dropTxtfile(workspace_ *cws, file_ *todropfile);

/*删除目录文件*/
void dropDirile(workspace_ *cws, file_ *todropfile);

/*删除文件（逻辑转换去调用该文件类型的删除函数）*/
void dropFile(workspace_ *cws, char *todropfilename);

/*打印目录的目录项*/
void ls(workspace_ *cws);

/*改变工作目录（change dir）*/
void cd(workspace_ *cws,char *destdirname);

/*系统操作逻辑（模拟shell命令操作）*/
void systemOprate(workspace_ *cws);



/*创建根目录*/
void createRootdir() {
	file_ rootdir;
	rootdir.type = 0;
	strcpy(rootdir.name, "root");
	strcpy(rootdir.fadir, "root");		//root的父目录是root
	strcpy(rootdir.cltauth, "7777");
	rootdir.time = gettime();
	rootdir.userpos = 0; 				//0*1000 + 0 == 0(默认根目录的创建者是根用户)
	memset(rootdir.nodealloc, 0, sizeof(rootdir.nodealloc));
	rootdir.nodealloc[0] = rootdir.nodealloc[1] = 1;
	rootdir.size = BLOCKSIZE;
	rootdir.wflag = 0;
	allocNode(&rootdir.nodepos,0);
	allocation[0][1] = allocation[0][2] = 1;
	
	memmove(nodes[0].content, &rootdir, FILESIZE);
	memmove(nodes[0].content + FILESIZE, &rootdir, FILESIZE);
}

//初始化位视图和存储节点空间，再初始化根目录
void systemInit() {
	initGroup();						//初始化groups
	createRootUser();					//创建root用户
	INIT_ALLOCATION  					//初始化分配表全为0
	INIT_NODESSIGN						//初始化节点的sign
	createRootdir();					//初始化根目录
}

/*获取的是年月日时间，int表示*/
int gettime() {
	time_t nowtime;
	time(&nowtime);
	struct tm *p;
	p = gmtime(&nowtime);
	return ((1900 + p->tm_year) * 10000 + (p->tm_mon+1) * 100 + p->tm_mday);
}

/*初始化传入的当前工作空间cws*/
void initWrokspace(workspace_ *cws){
	systemLogin(&cws->userpos);
	memmove(&cws->dirfile, nodes[0].content,FILESIZE);
}


/*在模拟的shell命令行中打印当前用户和当前工作目录*/
void print(workspace_ *cws){
	int igroup = cws->userpos/1000;
	int iuser = cws->userpos%1000;
	printf("[$%s ",groups[igroup].users[iuser].username);
	printf("%s/",cws->dirfile.name);
	printf("]: ");
}

/*将win文件存储的文件系统数据加载进内存正在运行的nodes[]中*/
void load(){
	FILE * pwinfile;
	pwinfile = fopen("store","r");
	if(pwinfile==NULL){
		printf("fail to load filesystem.\n");
		exit(-1);
	}
	fread(nodes,sizeof(int)+BLOCKSIZE,BLOCKCOUNT,pwinfile);
	fclose(pwinfile);
	memmove(groups,nodes[1].content,GROUPSPACESIZE);
	memmove(allocation,nodes[2].content,ALLOCATIONSPACESIZE);
	printf("load filesystem over!\n");
}


/*将内存正在运行的nodes[]存储的文件系统数据保存到win文件进中*/
void save(){
	memmove(nodes[1].content,groups,GROUPSPACESIZE);
	memmove(nodes[2].content,allocation,ALLOCATIONSPACESIZE);
	
	FILE * pwinfile;
	pwinfile = fopen("store","w");
	if(pwinfile==NULL){
		printf("fail to save filesystem.\n");
		
	}
	fwrite(nodes,sizeof(int)+BLOCKSIZE,BLOCKCOUNT,pwinfile);
	fclose(pwinfile);
	printf("save filesystem over!\n");
}

/*判断用户对文件是否有读权限，返回0或者1*/
int hasReadAutho(char *clkautho, int fileuserpos, int workuserpos) {
	if (fileuserpos == workuserpos%1000) {
		return (clkautho[0] - '0')%2;
	} else if (workuserpos / 1000 == 0)
		return (clkautho[1] - '0')%2;
	else if (workuserpos / 1000 == 1) {
		return (clkautho[2] - '0')%2;
	} else if (workuserpos / 1000 == 2) {
		return (clkautho[3] - '0')%2;
	}
}

/*判断用户对文件是否有写权限，返回0或者1*/
int hasWriteAutho(char *clkautho, int fileuserpos, int workuserpos) {
	if (fileuserpos == workuserpos%1000) {
		return (clkautho[0] - '0')%4 /2;
	} else if (workuserpos / 1000 == 0)
		return (clkautho[1] - '0')%4 /2;
	else if (workuserpos / 1000 == 1) {
		return (clkautho[2] - '0')%4 /2;
	} else if (workuserpos / 1000 == 2) {
		return (clkautho[3] - '0')%4 /2;
	}
}

/*判断当前用户是否能共享（ls能打印看到）该文件*/
int getShareAutho(char *clkautho, int fileuserpos, int workuserpos) {
	if (fileuserpos == workuserpos%1000) {
		return (clkautho[0] - '0')/4;
	} else if (workuserpos / 1000 == 0)
		return (clkautho[1] - '0')/4;
	else if (workuserpos / 1000 == 1) {
		return (clkautho[2] - '0')/4;
	} else if (workuserpos / 1000 == 2) {
		return (clkautho[3] - '0')/4;
	}
}

/*通过名字，在父目录块中找到子文件file_赋值给传入的subfile指针
	返回值是要找子文件在父目录的索引下标，返回值为负表示没找到*/ 
int getFileByName(char *filename,file_ *fafile, file_ *destfile){
	file_ tmpfile;
	int i;
	for(i=0; i<FILECOUNT; i++){
		if(i==1)	continue;
		memmove(&tmpfile,nodes[fafile->nodepos].content+i*FILESIZE,FILESIZE);
		if(fafile->nodealloc[i] && !strcmp(tmpfile.name, filename)){
			memmove(destfile,&tmpfile,FILESIZE);
			return i;	//返回i代表文件在父目录块中的位置
		}
	}
	printf("no such file to get.\n");
	return -1; 			//返回-1表示没找到
}


/*获取目录文件未分配目录项索引（非负有效）*/ 
int getFileFreeAllocIndex(int *alloc){ //大于1有效
	int i;
	for(i=2;i<FILECOUNT;i++){
		if(alloc[i]==0)	
			return i;
	}
	return -1;
}

/*通过绝对的文件路径找到file_并赋值给传入指针destfile(返回负值代表没找到)*/
int getFileByAbsPath(char *abspath, file_ *destfile){
	char *name[20];						//20表示文件路径最大深度
	int count = 0;
	int i = 0;
	const char delim[2] = "/";
	/*获取第一个子字符串*/
	name[i] = strtok(abspath, delim);
	/*继续获取其他的子字符串*/
	while( name[i] != NULL ) {
		count ++;
		name[++i] = strtok(NULL, delim);
	}
	
	i = 0;
	file_ tmpfile, rootfile;
	/*先寻找根目录*/
	memmove(&rootfile, nodes[0].content, FILESIZE);
	int result = getFileByName(name[0],&rootfile,&tmpfile);
	if(!SUCCESS(result))
		return -1;
	/*再寻找非根目录*/
	for(i=1; i<count; i++){
		result = getFileByName(name[i], &tmpfile, &tmpfile);
		if(!SUCCESS(result))
			return -1;
	}
	memmove(destfile, &tmpfile, FILESIZE);
	return 1;
}

/*相关文件同步创建文件带来的父目录分配表更改（祖父目录、父目录、兄弟目录：这些目录都存有对应父目录的file_）*/
int syncFileModification(file_ *fafile){
	file_ tmpfile;
	/*在祖父目录更新父目录对应的项*/
	int result = getFileByAbsPath(fafile->fadir, &tmpfile);
	if(!SUCCESS(result)){
		printf("grandpa sync failed 1.\n");
		return -1;
	}
	
	file_ vainfile;
	int index = getFileByName(fafile->name, &tmpfile, &vainfile);
	if(!SUCCESS(index)){
		printf("grandpa sync failed 2.\n");
		return -1;
	}
	memmove(nodes[tmpfile.nodepos].content + index*FILESIZE, fafile, FILESIZE);
	/*在父目录中更新.项*/
	memmove(nodes[fafile->nodepos].content, fafile, FILESIZE);
	/*在父目录的所有子目录中更新..项*/
	int i;
	for(i=2; i<FILECOUNT; i++){
		if(fafile->nodealloc[i] == 1){
			memmove(&tmpfile, nodes[fafile->nodepos].content + i*FILESIZE, FILESIZE);
			if(tmpfile.type == 0){
				memmove(nodes[tmpfile.nodepos].content + FILESIZE, fafile, FILESIZE);
			}
		}
	}
}


/*在父目录(当前工作目录)下创建文件*/
int createFile(workspace_ *cws, char *newfilename, int newfiletype) {
	
	/*创建并初始化文件*/ 
	file_ newfile;
	strcpy(newfile.name, newfilename);									//name
	newfile.type = newfiletype;											//type
	strcpy(newfile.cltauth, "7777");									//autho
	char fadir[PATHSIZE];
	if(strcmp(cws->dirfile.name, "root") != 0){			//父目录如果是根目录，则父目录的获取方式更变（不然就是"root/root"）
		strcat(fadir, cws->dirfile.fadir);
		strcat(fadir,"/");
	}
	strcat(fadir, cws->dirfile.name);
	strcpy(newfile.fadir, fadir);										//fadir
	newfile.time = gettime();											//time
	newfile.userpos = cws->userpos; 									//userpos
	memset(newfile.nodealloc, 0, sizeof(newfile.nodealloc));			//nodealloc
	newfile.size = BLOCKSIZE;											//size
	newfile.wflag = 0;													//wflag
	int nodeindex = getFreeNodeIndex();
	if(SUCCESS(nodeindex))								//有空闲块时才分配空间
		allocNode(&newfile.nodepos,nodeindex);							//nodepos
	else{
		printf("get no free node to alloc.\n");
		return -1;
	}
	
	int freeindex = getFileFreeAllocIndex(cws->dirfile.nodealloc);
	if(! SUCCESS(freeindex)){							//父目录对应块找不到空闲位置存储子文件的file_时
		printf("fadir has no space to create a fileitem.\n");
		return -1;
	}
	
	cws->dirfile.nodealloc[freeindex] = 1;
	if (newfile.type == 0){ 		
		newfile.nodealloc[0] = newfile.nodealloc[1] = 1;
		//新目录的.更新
		memmove(nodes[newfile.nodepos].content,&newfile,FILESIZE);
		//新目录的..块更新（其实在syncFileModification中更新父目录的所有子目录包括新建的这个子目录，这里不要也罢，留下是为了方便理解）
		memmove(nodes[newfile.nodepos].content+FILESIZE,&cws->dirfile,FILESIZE);
		//新目录写入父目录块
		memmove((nodes[cws->dirfile.nodepos].content+freeindex*FILESIZE),&newfile,FILESIZE);	
	}
	else if(newfile.type == 1){
		//新目录写入父目录块
		memmove((nodes[cws->dirfile.nodepos].content+freeindex*FILESIZE),&newfile,FILESIZE);
	}
	/*由于在父目录创建子文件父目录分配表更变，要写回物理块nodes[]（相对于本文件的），
		涉及凡是存有父目录file_的node都要更新（具体见同步函数sycnFileModification里的注释）*/
	
	int result = syncFileModification(&cws->dirfile);
	if(!SUCCESS(result)){
		/*事务回滚(失败处理):父目录的分配撤回*/
		cws->dirfile.nodealloc[freeindex] = 0;
		printf("create file failed.\n");
		return -1;
	}
	else{
		printf("create file over.\n");
		return 1;
	}
}

/*读块封装*/
void readBlock(int nodepos, char *buf, int readsize){
	memmove(buf,nodes[nodepos].content,readsize);
}

/*写块封装*/
void writeBlock(int nodepos, char *w_content, int writesize){
	memmove(nodes[nodepos].content,w_content,writesize);
}

/*写指定内容w_content进指定pfile(cws的工作目录还是文本文件pfile的父目录)*/
int writeFile(file_ *pfile, char *w_content,workspace_ *cws){
	/*无写权限*/
	if(!hasWriteAutho(pfile->cltauth,pfile->userpos,cws->userpos)){
		printf("you have no write authority.\n");
		return -1;
	}
	/*已经有用户在写该文件*/
	if(pfile->wflag==1){
		printf("the other user is writing it");
		return -1;
	}
	/*该文件是目录文件，不能写*/
	if(pfile->type==0){			
		printf("can not write a dir.\n");
		return -1;
	}
	pfile->wflag = 1;					// 修改文件写标志
	/*计算要写内容对应的块数*/
	int contentsize = strlen(w_content);
	int blockcount = contentsize/BLOCKSIZE;
	int restsize = contentsize%BLOCKSIZE;
	if(restsize>0)	blockcount++;
	
	/*直接级块够用时如下
		注：通过pfile->pnode->content块间的前10个直接块（共有15个小块，每个小块存一个nodepos指向具体node，指向直接/1/2/3级块）*/
	if(blockcount<=DLEVELCOUNT){
		int i;
		for(i=0;i<blockcount-1;i++){
			int freeindex = getFreeNodeIndex();
			int newnodepos ;
			allocNode(&newnodepos,freeindex);
			writeBlock(newnodepos, w_content+i*BLOCKSIZE, BLOCKSIZE);
			memmove(nodes[pfile->nodepos].content+i*FILESIZE,&newnodepos, sizeof(int));
			pfile->nodealloc[i] = 1;
		}
		/*最后的内容写不满一个块,特殊处理*/
		int freeindex = getFreeNodeIndex();
		int	newnodepos;
		allocNode(&newnodepos,freeindex);
		writeBlock(newnodepos,w_content+i*BLOCKSIZE,restsize);
		memmove(nodes[pfile->nodepos].content+i*FILESIZE, &newnodepos, sizeof(int));
		pfile->nodealloc[i] = 1;
	}
	//	待实现
	//  需要一级索引块时
	//	else if(blockcount<FLEVELCOUNT*INDEXESCOUNT+DLEVELCOUNT){}
	//	需要二级索引块时
	//	else if(blockcount<SLEVELCOUNT*INDEXESCOUNT*INDEXESCOUNT+DLEVELCOUNT+FLEVELCOUNT*INDEXESCOUNT){}
	//	需要三级索引块时
	//	else if(blockcount<TLEVELCOUNT*INDEXESCOUNT*INDEXESCOUNT*INDEXESCOUNT+SLEVELCOUNT*INDEXESCOUNT*INDEXESCOUNT+DLEVELCOUNT+FLEVELCOUNT*INDEXESCOUNT){}
	else
		printf("can not support too large textfile like this.\n");
	
	pfile->wflag = 0;//写完值写标志为1（相当于释放锁）
	
	/*pfile的分配表有变化，将文本文件pfile内容写入其在父目录对应的块*/
	file_ vainfile;
	int offet = getFileByName(pfile->name, &cws->dirfile, &vainfile);		//获得要写pfile在父目录块的位置(肯定是能找到的，不做异常处理)
	memmove(nodes[cws->dirfile.nodepos].content+offet*FILESIZE, pfile, FILESIZE);
	memmove(&vainfile, nodes[pfile->nodepos].content+offet*FILESIZE, FILESIZE);
	printf("write done.\n");
}

/*将文本文件pfile的内容打印到屏幕*/
void readFile(file_ *pfile,int workuserpos){
	int nodepos = pfile->nodepos;
	/*判断读权限*/
	if(!hasReadAutho(pfile->cltauth,pfile->userpos,workuserpos)){
		printf("you have no read authority.\n");
		return;
	}
	
	char buf[BLOCKSIZE+1];
	buf[BLOCKSIZE] = '\0';		//手动配置结束符，防止要读文件特别大一次读取读不到'\0'时打印出错
	
	/*读1级块内容*/ 
	int i;
	for(i=0;i<DLEVELCOUNT;i++){
		if(pfile->nodealloc[i]){
			int tmpnodepos;
			memmove(&tmpnodepos,nodes[nodepos].content+i*FILESIZE,sizeof(int));
			readBlock(tmpnodepos,buf,BLOCKSIZE);
			printf("%s",buf);
		}
	}
	//读1/2/3级块待实现
	//	
	//
}

/*删除文本文件*/
void dropTxtfile(workspace_ *cws, file_ *todropfile){
	file_ vainfile;
	int index = getFileByName(todropfile->name, &cws->dirfile, &vainfile);
	cws->dirfile.nodealloc[index] = 0;
	allocation[todropfile->nodepos / 32][todropfile->nodepos % 32] = 0;
	//父目录分配表更变,同步更新
	syncFileModification(&cws->dirfile);
}

/*删除目录文件*/
void dropDirile(workspace_ *cws, file_ *todropfile){
	file_ tmpfile;
	int i;
	for(i=2; i<FILECOUNT; i++){
		if(todropfile->nodealloc[i] == 1){
			memmove(&tmpfile, nodes[todropfile->nodepos].content + i*FILESIZE, FILESIZE);
			if(tmpfile.type == 0){	//递归删除子目录
				cd(cws, todropfile->name);
				dropDirile(cws, &tmpfile);
			}	
			else if(tmpfile.type == 1){
				cd(cws, todropfile->name);
				dropTxtfile(cws, &tmpfile);
				cd(cws, "..");
			}
		}
	}
	file_ vainfile;
	int index = getFileByName(todropfile->name, &cws->dirfile, &vainfile);
	cws->dirfile.nodealloc[index] = 0;
	allocation[todropfile->nodepos / 32][todropfile->nodepos % 32] = 0;
	//父目录分配表更变,同步更新
	syncFileModification(&cws->dirfile);
}

/*删除文件（逻辑转换去调用该文件类型的删除函数）*/
void dropFile(workspace_ *cws, char *todropfilename){
	file_ tmpfile;
	int result = getFileByName(todropfilename, &cws->dirfile, &tmpfile);
	if(!SUCCESS(result))
	if(tmpfile.type == 0)
		dropDirile(cws, &tmpfile);
	else if(tmpfile.type == 1)
		dropTxtfile(cws, &tmpfile);
	printf("drop over.\n");
}

/*打印目录的目录项*/
void ls(workspace_ *cws) {
	printf("name:\t\ttype:\t\ttime:\t\t\tsize:\n");
	file_ tmpfile;
	int i;
	for (i = 2; i < FILECOUNT; ++ i) {				//i=2是因为默认不输出目录的.和..项
		if (cws->dirfile.nodealloc[i]==1) {
			memmove(&tmpfile, nodes[cws->dirfile.nodepos].content + i * FILESIZE, FILESIZE);
			if (hasReadAutho(tmpfile.cltauth, tmpfile.userpos, cws->dirfile.userpos))
				printf("%s\t\t%d\t\t%d\t\t%d\n", tmpfile.name, tmpfile.type, tmpfile.time, tmpfile.size);
		}
	}
}

/*改变工作目录（change dir）*/
void cd(workspace_ *cws,char *destdirname){
	file_ tmpfile;
	if(!strcmp(destdirname,"..")){
		memmove(&tmpfile,nodes[cws->dirfile.nodepos].content + FILESIZE,FILESIZE);
		cws->dirfile = tmpfile;
		cws->dirfile.nodepos = tmpfile.nodepos;
		return;
	}
	int i;
	for(i=1;i<FILECOUNT;i++){
		memmove(&tmpfile,nodes[cws->dirfile.nodepos].content+i*FILESIZE,FILESIZE);
		
		if(!strcmp(tmpfile.name,destdirname) && tmpfile.type==0 && cws->dirfile.nodealloc[i]){
			cws->dirfile = tmpfile;
			cws->dirfile.nodepos = tmpfile.nodepos;
			return;
		}
	}
	printf("no such dir to change.\n");
}

/*系统操作逻辑（模拟shell命令操作）*/
void systemOprate(workspace_ *cws){
	char command[64];
	while(1){
		print(cws);
		scanf("%[^\n]",command);
		char *argv[5];
		int argc = 0;
		int i = 0;
		const char delim[2] = " ";
		/* 获取第一个子字符串 */
		argv[i] = strtok(command, delim);
		/* 继续获取其他的子字符串 */
		while( argv[i] != NULL ) {
			argc ++;
			argv[++i] = strtok(NULL, delim);
		}

		
		if(argv[0]==NULL){									//只接收到回车什么都不做
			getchar();
			continue;
		}
		else if (!strcmp(argv[0],"ls") && argc==1){			//ls
			ls(cws);										//打印当前工作目录目录项（list）
			getchar();
		}
		else if	(!strcmp(argv[0],"cat") && argc==3){		//cat [filename] [filetype]	
			int filetype = atoi(argv[2]);					//创建文件（create）
			if(filetype==0 || filetype==1)
				createFile(cws,argv[1],filetype);
			else
				printf("can not create file by this filetype.\n");
			getchar();
		}
		else if(!strcmp(argv[0],"drop") && argc==2){		//drop [filename]
			dropFile(cws, argv[1]);							//删除文件
			getchar();
		}
		else if	(!strcmp(argv[0],"vi") && argc==2){			//vi [filename]
			file_ readfile;									//查看文本文件内容
			int result = getFileByName(argv[1], &cws->dirfile, &readfile);
			if(SUCCESS(result))
				readFile(&readfile,cws->userpos);
			getchar();
		}
		else if	(!strcmp(argv[0],"wrt") && argc==2){		//wrt [filename]
			file_ writefile;								//写文件
			int result = getFileByName(argv[1], &cws->dirfile, &writefile);
			if(SUCCESS(result)){
				char buf[1024];				//！！！不能永远只读1024个吧	//有待改进
				printf("text content:\n");
				getchar();
				scanf("%[^EOF]",buf)	;
				writeFile(&writefile,buf,cws);
			}
		}
		else if	(!strcmp(argv[0],"cd") && argc==2){			//cd [dirname]
			cd(cws, argv[1]);								//切换工作目录（只能到当前工作目录的父目录和子目录）（change dir）
			getchar();
		}
		else if	(!strcmp(argv[0],"exit") && argc==1)		//exit
			break;											//退出系统
		else if	(!strcmp(argv[0],"catuser") && argc==1){	//catuser
			createUser();									//创建用户
			getchar();
		}
		else if	(!strcmp(argv[0],"dropuser") && argc==1){	//dropuser
			dropUser();										//删除用户
			getchar();
		}
		else{
			printf("no such command.\n");
			getchar();
		}
	}
}


