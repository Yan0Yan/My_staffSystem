/*  该文件用于：
 *  系统库头文件的预处理声明
 *  宏变量定义
 *  错误处理宏函数定义
 *  私有结构体声明
 *  模式枚举类型声明
 *  传输协议结构体声明
*/
#ifndef __MY_HEAD__
#define __MY_HEAD__


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>

#include <pthread.h>
#include <sqlite3.h>


#define STAFF_MAX       50                          //StaffData结构体字符串成员长度
#define HISTORY_MAX     128                         //HistoryData结构体字符串成员长度

#define CLIENTDATA      ((ClientData*)clientData)   //在线程函数中用于增加线程函数参数的可读性
#define SUPER_PWD       "666-888-233"               //当有新的管理员用户注册申请时 用户必须输入正确的超级密码

#define SERVER_IP       "192.168.250.100"           //服务器局域网ip
#define SERVER_PORT     8888                        //服务器端口号

#define TRANSFER_SIZEOF 512                         //传输大小
#define SQL_STR_MAX     512                         //数据库命令字符串长度

#define SQL_ARG_MODE    (((Sql_arg*)arg)->mode)     //在查询回调函数中用于增加查询模式参数的可读性
#define SQL_ARG_CLIENT  (((Sql_arg*)arg)->client)   //在查询回调函数中用于增加的客户端信息参数的可读性

#define INPUT_MAX       50                          //客户端中 用户一次输入数据的大小

//客户端内服务器关闭连接的情况 直接退出客户端
#define SERVER_CLOSE {                                              \
    fprintf(stderr, "很抱歉 服务器下线或者遇到错误 客户端退出\n");     \
    exit(-1);                                                       \
}

//客户端出现致命错误 需要主动关闭连接 直接退出客户端
#define ERR_CLIENT_CLOSE(msg) {                                     \
    fprintf(stderr, "%s : %d\n", __FILE__, __LINE__);               \
    perror(msg);                                                    \
    fprintf(stderr, "很抱歉 客户端出现致命错误 客户端退出\n");         \
    exit(-1);                                                       \
}


//服务器main.c内：服务器搭建失败后退出程序 返回-1
#define ERR_SERVER(mess) {                                          \
    fprintf(stderr, "%s : %d\n", __FILE__, __LINE__);               \
    perror(mess);                                                   \
    return -1;                                                      \
}

//服务器sql.c内：处理数据库操作中出现致命错误 退出函数
#define ERR_SQL(db) {                                               \
    fprintf(stderr, "%s : %d\n", __FILE__, __LINE__);               \
	fprintf(stderr, "%s\n", sqlite3_errmsg(db));                    \
	return -1;                                                      \
}

//服务器sql.c和服务器server.c内：与客户端交互中出现致命错误 主动关闭连接 释放堆空间 退出交互线程函数
#define ERR_CLIENT(cindata) {                                       \
    fprintf(stderr, "%s : %d\n", __FILE__, __LINE__);               \
    PRINT_CLIENT(cindata);                                          \
    fprintf(stderr, "客户端非正常关闭\n");                           \
    close(cindata->newfd);                                          \
    free(cindata);                                                  \
    pthread_exit(NULL);                                             \
}

//服务器server.c内：客户端主动关闭了连接 服务器关闭套接字 释放堆空间 退出交互线程函数
#define EXIT_CLIENT(cindata) {                                      \
    fprintf(stderr, "%s : %d\n", __FILE__, __LINE__);               \
    PRINT_CLIENT(cindata);                                          \
    fprintf(stderr, "客户端主动关闭连接\n");                          \
    close(cindata->newfd);                                          \
    free(cindata);                                                  \
    pthread_exit(NULL);                                             \
}

//服务器server.c内：在线程函数与客户端交流中 打印与之交互的客户端的信息
#define PRINT_CLIENT_PTHREAD(cindata) {                             \
    fprintf(stderr, "[ %2d : %s : %4d ]: ",                         \
        ((ClientData*)cindata)->newfd,                              \
        inet_ntoa(((ClientData*)cindata)->cin.sin_addr),            \
        ntohs(((ClientData*)cindata)->cin.sin_port)                 \
    );                                                              \
}

//服务器sql.c和服务器server.c内：打印与之交互的客户端信息
#define PRINT_CLIENT(cindata) {                                     \
    fprintf(stderr, "[ %2d : %s : %4d ]: ",                         \
            cindata->newfd,                                         \
            inet_ntoa(cindata->cin.sin_addr),                       \
            ntohs(cindata->cin.sin_port)                            \
    );                                                              \
}

//用户注册或登录的信息结构体 会被用于传输
typedef struct {
    char    _uid[STAFF_MAX];            //用户名 (6位数 首位只能为0和1 0是普通用户 1是管理员用户)        
    char    _pwd[STAFF_MAX];            //密码   (5-20位 不能使用英文和英文字符以外的字符)
} UserData;

//员工信息结构体 会被用于传输
typedef struct {
    UserData    _user;                  //员工登录时的用户名和密码 (对其他员工隐藏)
    char        name[STAFF_MAX];        //员工姓名 
    char        sex[STAFF_MAX];         //员工性别
    char        post[STAFF_MAX];        //员工职位
    char        phone[STAFF_MAX];       //员工电话
    char        _money[STAFF_MAX];      //员工月薪 (对其他普通员工隐藏)
    char        _addr[STAFF_MAX];       //员工住址 (对其他普通员工隐藏)
    char        _birth[STAFF_MAX];      //员工生日 (对其他普通员工隐藏)
    char        _hiredata[STAFF_MAX];   //员工入职日期 (对其他普通员工隐藏)
} StaffData;

//修改员工信息结构体 会被用于传输
typedef struct {
    char newData[STAFF_MAX];
    char type[STAFF_MAX];
    char name[STAFF_MAX];
    char reviser[STAFF_MAX];         //修改人
} ReviseData;

//历史记录结构体 会被用于传输
typedef struct {
    char time[HISTORY_MAX];             //事件时间
    char history[HISTORY_MAX];          //事件详情
} HistoryData;

//要传入线程函数的参数结构体 服务器私有
typedef struct {
    int                 newfd;      //该客户端的文件描述符
    struct sockaddr_in  cin;        //该客户端的信息结构体
    sqlite3             **db;       //服务器所维护的数据库指针
} ClientData;


//用户在welcome_init()所选模式的枚举 
enum ModeInit {
    INIT_LOGOUT = '0',
    INIT_LOGON,
    INIT_REGISTER
};
//用户在welcome_root()所选模式的枚举
enum ModeRoot {
    ROOT_LOGOUT = '0',
    ROOT_ADD,
    ROOT_DELETE,
    ROOT_REVISE,
    ROOT_QUERY_STAFF,
    ROOT_QUERY_STAFF_HISTORY,
    ROOT_QUERY_STAFF_ALL
};
//用户在welcome_normal()所选模式的枚举
enum ModeNormal {
    NORMAL_LOGOUT = '0',
    NORMAL_REVISE,
    NORMAL_QUERY_SELF,
    NORMAL_QUERY_HISTORY,
    NORMAL_QUERY_STAFF,
    NORMAL_QUERY_STAFF_ALL
};

//服务器回复枚举 用于提示客户端
enum Answer {
    QUERY_NO_FIND = '0',    //用户查询：没有找到数据 
    QUERY_OK,               //用户查询：已找到数据
    QUERY_END,              //用户查询：数据已经全部列举完毕
    LOGON_FAIL_UID,         //用户登录：uid不存在
    LOGON_FAIL_PWD,         //用户登录：密码错误
    LOGON_OK,               //用户登录：登录成功
    REGISTER_FAIL_UID,      //用户注册：uid已存在
    REGISTER_OK,            //用户注册：注册成功
    DELETE_FAIL,            //管理员删除员工：  失败
    DELETE_OK,              //管理员删除员工：  成功
    REVISE_FAIL,            //用户请求修改数据库的数据：失败
    REVISE_OK               //用户请求修改数据库的数据：成功
};

/* 给查询回调函数参数void *arg传递的值
 * 前四位表示模式：
 * 0001：   登录
 * 0010：   注册
 * 0100：   查询员工信息
 * 1000：   查询历史记录
 * 第五位表示查询人员是否为管理员
 * 0：      普通员工
 * 1：      管理员
 * 第六位表示查询员工信息时 是否有权查看该员工的隐藏信息
 * 0：      无
 * 1：      有
 * 第七位表示登录时密码有无错误
 * 0：      无误
 * 1：      错误
 * 最高位表示回调函数是否至少执行过一次
 * 0：      无
 * 1：      有
*/
enum Sql_queryMode {
    SQL_QUERY_LOGON     = 0b00000001,
    SQL_QUERY_REGISTER  = 0b00000010,
    SQL_QUERY_STAFF     = 0b00000100,
    SQL_QUERY_HISTORY   = 0b00001000,
    SQL_QUERY_ROOT      = 0b00010000,
    SQL_QUERY_SHOW_ALL  = 0b00100000,
    SQL_QUERY_PWD_ERROR = 0b01000000,
    SQL_QUERY_YES       = 0b10000000,
    SQL_QUERY_NO        = 0b00000000    //与YES对应 入参时默认没执行过       
};
//传送给sql_callbackQuery中的arg参数 服务器私有
typedef struct {
    int                 mode;           //查询模式
    ClientData          *client;        //需要与之交互的客户端的信息
    char                *p_user;        //所需的uid或者用户名
    char                *p_pwd;         //只有登录操作才需要该指针
    char                *p_query;       //查询信息的人员
} Sql_arg;

//服务器发送协议 (客户端接收协议)
typedef struct {
    enum Answer answer;                 //模式头             
    union {                             //数据包   
        HistoryData historyData;    
        StaffData   staffData;       
    };
    int flag;                           //防止粘包 无数据意义
} Msg_serverToClient;

//客户端发送协议 (服务器接收协议)
typedef struct {
    union {                             //模式头
        enum ModeInit    modeInit;
        enum ModeRoot    modeRoot;
        enum ModeNormal  modeNormal;
    };
    union {                             //数据包
        UserData    userData;  
        StaffData   staffData;  
        ReviseData  reviseData; 
    };
    int flag;                           //防止粘包 无数据意义
} Msg_clientToServer;


#endif
