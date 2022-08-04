/*用于数据库的维护*/
#ifndef __SQL__
#define __SQL__


#include "../my_head.h"
#include "server.h"

//初始化数据库
int sql_init(sqlite3 **db);

//导入员工数据
int sql_staffAdd(ClientData *clientData, StaffData staffData);
//删除员工数据
int sql_staffDelete(ClientData *clientData, char name[]);
//修改员工数据
int sql_staffRevise(ClientData *clientData, const char *user, ReviseData reviseData);
//查询功能的回调函数
int sql_callbackQuery(void *arg, int len, char **text, char **type);

//生成当前系统时间字符串
static void get_now_time(char now_time[]);


#endif
