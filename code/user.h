#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<malloc.h>


#define USERNAMESIZE 16					//用户名最大字节数
#define GROUPSIZE 16					//每组最大用户数 
#define GROUPSPACESIZE 1788  			//全局变量groups的大小(方便从blocks[1]中存取groups)

/*用户*/
typedef struct{
    char username[USERNAMESIZE];
    char password[USERNAMESIZE];
    int groupid;
}user_;

/*组*/
typedef struct {
    char groupname[USERNAMESIZE];
    user_ users[GROUPSIZE];
    int ucount;    
}group_;


group_ groups[3];
//groups[0]---ROOTGROUP 
//groups[1]---FORMALGROUP 
//groups[2]---OHTERGROUP 2

/*创建用户（创建的用户直接存在全局变量groups[]中）*/
void createUser(){
    user_ newuser;
    printf("username: ");
    scanf("%s",newuser.username);
    printf("password: ");
    scanf("%s",newuser.password);
    printf("groupid: ");
    scanf("%d",&newuser.groupid);
    
    //checkUserRignt(&newuser);				//检查用户名和密码的合法性（可以实现一下）		
    int index = groups[newuser.groupid].ucount;
    if(index < GROUPSIZE){
        groups[newuser.groupid].users[index] = newuser;
        groups[newuser.groupid].ucount++;
    }
    printf("create user over.\n");
}

/*在groups[]寻找用户（返回用户的pos，返回负数时代表没找到）*/
int findUser(char *username, char *password){
    int i,j,groupid=-1,index=-1;
    for(i=0;i<3;i++){
        int isfound = 0;
        for(j=0;j<groups[i].ucount;j++){
            int issamename = !strcmp(username,groups[i].users[j].username);
            int issamepassword = !strcmp(password,groups[i].users[j].password);
            if(issamename && issamepassword){
                isfound = 1;
                groupid = i;
                index = j;
                break;
            }
        }
        if(isfound) break;
    }
	//返回的usrpos==组号*1000+组内下标
    return groupid*1000 + index;
}

/*删除用户（不做内存释放，修改分配表置零代表删除）*/
void dropUser(){
    char username[USERNAMESIZE],password[USERNAMESIZE];
    printf("username: ");
    scanf("%s",username);
    printf("password: ");
    scanf("%s",password);

    int pos = findUser(username, password);
    if(pos > 0 ){ //rootuser 的pos为0，所有用户都不能删除rootuser 
        int groupid = pos/1000;
        int index = pos%1000;
        int i;
        for(i=index;i<groups[groupid].ucount - 1;i++)
            groups[groupid].users[i] = groups[groupid].users[i+1];
        groups[groupid].ucount--;
    	printf("drop user over.\n");
    }
    else
    	printf("no such user.\n");
}

/*初始化全局变量groups[]*/
void initGroup(){
	groups[0].ucount = 0;
	groups[1].ucount = 0;
	groups[2].ucount = 0;
	strcpy(groups[0].groupname,"ROOTGROUP");
	strcpy(groups[1].groupname,"FORMALGROUP");
	strcpy(groups[2].groupname,"OHTERGROUP");
}

/*在groups[0]的第一个用户创建root用户*/
void createRootUser(){
	user_ rootuser;
	strcpy(rootuser.username,"root");
	strcpy(rootuser.password,"123456");
	rootuser.groupid = 0;
	
	groups[0].users[0] = rootuser;
	groups[0].ucount++;
}

/*用户登录 （显示登录界面并返回界面录入的用户在groups的位置，pos为负时表示找不到，无法登录）*/ 
int login(){
	char username[USERNAMESIZE],password[USERNAMESIZE];
	printf("username:");
	scanf("%s",username);
	printf("password:");
	scanf("%s",password);
	getchar();//接收回车防止影响后面接收命令
	int pos = findUser(username,password);
	 
	return pos; // 非负时表示找到用户
} 

/*系统登录（对传入的当前工作用户赋值，并返回登录结果0或者1）*/
int systemLogin(int *cwsuserpos){
	int userpos;
	while((userpos = login()) < 0);
	*cwsuserpos = userpos;
	return (userpos>=0);
}
