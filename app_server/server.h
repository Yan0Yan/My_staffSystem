/*该服务器为TCP多线程并发服务器*/
#ifndef __SERVER__
#define __SERVER__


#include "../my_head.h"

//初始化服务器
int server_set(int *sfd, struct sockaddr_in *sin, socklen_t addrlen);
//初始化传入线程函数的参数结构体
ClientData *client_setPthreadData(int newfd, struct sockaddr_in cin, sqlite3 **db);
//服务器与客户端交互线程函数
void *client_pthread(void *clientData);

//用户登录函数
static void client_logon(ClientData *clientData, UserData userData);
//用户注册函数
static void client_register(ClientData *clientData, UserData userData);
//用户登录成功后 与管理员用户交互的函数
static int client_root(ClientData *clientData, UserData rootData);
//用户登录成功后 与普通员工用户交互的函数
static int client_staff(ClientData *clientData, UserData staffData);


#endif