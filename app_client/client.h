//该客户端为TCP客户端
#ifndef __CLIENT__
#define __CLIENT__


#include "ui.h"
#include "../my_head.h"

//与服务器进行连接
int connect_server(int *sfd, struct sockaddr_in *sin, int addr_len);

//登录之前与用户交互的函数
int user_init(int sfd);
//处理用户注册函数
static void user_logon(int sfd);
//处理用户登录函数
static void user_register(int sfd);

//管理员身份登录成功之后的与用户交互的函数
static int root_logon(int sfd, char username[]);
//普通用户身份登录成功之后的与用户交互的函数
static int staff_logon(int sfd, char username[]);

//管理员：添加员工信息的操作
static void root_add_staff(int sfd, UserData userData);
//管理员：删除某员工所有信息
static void root_delete_staff(int sfd);
//管理员：修改某员工的某个信息
static void root_revise_staff(int sfd, char username[]);
//管理员：查询某员工的信息
static void root_query_staff(int sfd, char username[]);
//管理员：查询某员工的历史记录
static void root_query_history(int sfd, char username[]);
//管理员：查询所有员工的信息
static void root_query_all(int sfd, char username[]);


//普通员工：修改自己的某个信息
static void normal_revise_staff(int sfd, char username[]);
//普通员工：查询自己的员工的信息
static void normal_query_self(int sfd, char username[]);
//普通员工：查询别的员工的信息
static void normal_query_staff(int sfd, char username[]);
//普通员工：查询自己的历史记录
static void normal_query_history(int sfd, char username[]);
//普通员工：查询所有员工的信息
static void normal_query_all(int sfd, char username[]);


#endif