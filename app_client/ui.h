/*该文件用于显示ui界面*/
#ifndef __UI__
#define __UI__


#include "../my_head.h"

//程序欢迎界面
void welcome_init(void);
//管理员欢迎界面
void welcome_root(const char name[]);
//普通用户欢迎界面
void welcome_normal(const char name[]);


#endif