#include "server.h"
#include "sql.h"

/***********************************************
 * 功能：
 *      搭建TCP服务器
 * 参数：
 *      sfd：       套接字的文件描述符指针
 *      sin：       服务器信息结构体指针
 *      addrlen：   服务器信息结构体长度
 * 返回：
 *      -1：搭建失败
 *       0：搭建成功
 ***********************************************/
int server_set(int *sfd, struct sockaddr_in *sin, socklen_t addrlen)
{
    printf("初始化服务器......\n");

    *sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (*sfd < 0) {
        ERR_SERVER("socket");
    }
    printf("创建流式套接字成功\n");

    int reuse = 1;
    if (setsockopt(*sfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        ERR_SERVER("setsockopt");
    }
    printf("允许端口快速重用\n");

    sin->sin_family         = AF_INET;
    sin->sin_port           = htons(SERVER_PORT);
    sin->sin_addr.s_addr    = inet_addr(SERVER_IP);
    if (bind(*sfd, (struct sockaddr*)sin, addrlen) < 0) {
        ERR_SERVER("bind");
    }
    printf("服务器信息bind成功\n");

    if (listen(*sfd, 10) < 0) {
        ERR_SERVER("lislen");
    }
    printf("设置监听状态成功\n\n");

    return 0;
}

/***********************************************
 * 功能：
 *      初始化传入线程函数的参数结构体
 * 参数：
 *      newfd：     刚与之成功建立连接的客户端的文件描述符
 *      cin：       刚与之成功建立连接的客户端的端口地址结构体
 *      db：        服务器所维护的数据库指针
 * 返回：
 *      NULL：          失败
 *      p_new_client：  堆申请出的客户端信息结构体地址
 ***********************************************/
ClientData *client_setPthreadData(int newfd, struct sockaddr_in cin, sqlite3 **db)
{
    ClientData *p_new_client = NULL;

    //堆申请的客户端信息结构体 由该函数申请 释放则由与之交互的线程函数完成
    p_new_client = (ClientData*)malloc(sizeof(ClientData));
    if (NULL == p_new_client) {
        perror("malloc");
        return NULL;
    }

    p_new_client->newfd   = newfd;
    p_new_client->cin     = cin;
    p_new_client->db      = db;

    return p_new_client;
}

/***********************************************
 * 功能：
 *      用于与客户端交互的线程函数
 *      并负责处理客户端处于注册登录前的交互
 * 参数：
 *      clientData：客户端的信息结构体 (是堆空间的 需要free!)
 *                  使用时需要强转成(ClientData*)clientData 可使用宏CLIENTDATA代替它   
 * 返回：
 *      NULL
 * 备注：
 *      此时客户端进入init_welcome() 界面为：
 *          1. 登录
 *          2. 注册
 *          0. 退出
 ***********************************************/
void *client_pthread(void *clientData)
{
    pthread_detach(pthread_self());
    PRINT_CLIENT(CLIENTDATA);
    printf("与该客户端连接成功\n");

    int         res             = -1;   //接收recv和send的返回值的变量
    Msg_clientToServer 		buf_recv;   //接收用户发送的信息
	Msg_serverToClient  	buf_send;   //发送给用户的信息

    //正式与客户端交互 用户进入init_welcome界面
    while (1)
    {
        res = recv(CLIENTDATA->newfd, &buf_recv, sizeof(buf_recv), 0);
		if (0 > res) {
			perror("recv_1");
            ERR_CLIENT(CLIENTDATA);
		}
		if (0 == res) {
			EXIT_CLIENT(CLIENTDATA);
		}

        switch (buf_recv.modeInit)
        {
        case INIT_LOGOUT:
            EXIT_CLIENT(CLIENTDATA);
            break;

        case INIT_LOGON:
            PRINT_CLIENT(CLIENTDATA);
            printf("进入登录模式\n");
            client_logon(CLIENTDATA, buf_recv.userData);
            break;

        case INIT_REGISTER:
            PRINT_CLIENT(CLIENTDATA);
            printf("进入注册模式\n");
            client_register(CLIENTDATA, buf_recv.userData);
            break;

        default:
            PRINT_CLIENT(CLIENTDATA);
            printf("数据解析有误 %d\n", buf_recv.modeInit);
        }
    }
}

/***********************************************
 * 功能：
 *      用于处理用户注册
 * 参数：
 *      clientData：客户端的信息结构体
 *      userData：  用户提供的注册信息
 * 返回：
 *      void
 ***********************************************/
static void client_register(ClientData *clientData, UserData userData)
{
    int res     = -1;
    int res_sql = -1;
    Sql_arg  sql_arg; 
    Msg_serverToClient buf_send;    
    Msg_clientToServer buf_recv;      
    char buf_sql_str[SQL_STR_MAX]       = "";
    const char  sql_str[SQL_STR_MAX]    = "select * from Staff where _uid=%s;";
    
    memset (&buf_send, 0, sizeof(buf_send));
    memset (&sql_arg, 0, sizeof(sql_arg));

    sql_arg.client  = clientData;
    sql_arg.mode    = SQL_QUERY_REGISTER;
    sql_arg.p_pwd   = userData._pwd;

    sprintf(buf_sql_str, sql_str, userData._uid);
    res_sql = sqlite3_exec(*(clientData->db), buf_sql_str, sql_callbackQuery, &sql_arg, NULL);
    if (0 != res_sql && 1 != res_sql) {
        fprintf(stderr, "%s\n", sqlite3_errmsg(*(clientData->db)));
		EXIT_CLIENT(clientData);
	}

    //如果进入了该语句 则说明没有与之对应的用户uid 可以进行注册
    if (0 == (sql_arg.mode & SQL_QUERY_YES)) {
        buf_send.answer = REGISTER_OK;

        PRINT_CLIENT(clientData);
        printf("注册账号成功\n");
    }
    //否则 则说明有与之对应的用户uid 不可以进行注册
    else {
        buf_send.answer = REGISTER_FAIL_UID;

        PRINT_CLIENT(clientData);
        printf("用户名已存在 注册账号失败\n");

        res = send(clientData->newfd, &buf_send, sizeof(buf_send), 0);
        if (0 > res) {
            perror("send_00");
            ERR_CLIENT(clientData);
        }

        return;
    }

    res = send(clientData->newfd, &buf_send, sizeof(buf_send), 0);
    if (0 > res) {
        perror("send_0");
        ERR_CLIENT(clientData);
    }


    //注册成功之后 则需要该用户自己添加自己的信息
    res = recv(clientData->newfd, &buf_recv, sizeof(buf_recv), 0);
    if (0 > res) {
        perror("recv_0");
        ERR_CLIENT(clientData);
    }
    if (0 == res) {
        EXIT_CLIENT(clientData);
    }

    if (sql_staffAdd(clientData, buf_recv.staffData)) {
        ERR_CLIENT(clientData);
    }  
}

/***********************************************
 * 功能：
 *      用于处理用户登录
 *      如果登录失败 上级函数会继续阻塞等待用户重新选择模式
 *      如果登录成功 则根据用户uid首位跳转到对应的交互模式函数
 * 参数：
 *      clientData：客户端的信息结构体
 *      userData：  用户提供的注册信息
 * 返回：
 *      void
 * 备注：
 *      用户的uid就是用户名 
 *      根据uid首位是0还是1区分
 ***********************************************/
static void client_logon(ClientData *clientData, UserData userData)
{
    int res     = -1;
    int res_sql = -1;
    Sql_arg  sql_arg; 
    Msg_serverToClient buf_send;          
    char buf_sql_str[SQL_STR_MAX]       = "";
    const char  sql_str[SQL_STR_MAX]    = "select * from Staff where _uid=\"%s\";";
    
    memset (&buf_send, 0, sizeof(buf_send));
    memset (&sql_arg, 0, sizeof(sql_arg));

    sql_arg.client  = clientData;
    sql_arg.mode    = SQL_QUERY_LOGON;
    sql_arg.p_user  = userData._uid;
    sql_arg.p_pwd   = userData._pwd;

    sprintf(buf_sql_str, sql_str, userData._uid);
    res_sql = sqlite3_exec(*(clientData->db), buf_sql_str, sql_callbackQuery, &sql_arg, NULL);
    if (0 != res_sql && 1 != res_sql) {
        fprintf(stderr, "%s\n", sqlite3_errmsg(*(clientData->db)));
		ERR_CLIENT(clientData);
	}

    //如果进入该语句 说明根本没有执行查询回调函数 表示用户名不存在
    if (0 == (sql_arg.mode & SQL_QUERY_YES)) {
        buf_send.answer = LOGON_FAIL_UID;

        res = send(clientData->newfd, &buf_send, sizeof(buf_send), 0);
        if (0 > res) {
            perror("send_1");
            ERR_CLIENT(clientData);
        }
        PRINT_CLIENT(clientData);
        printf("登录用户名不存在\n");
    }

    //如果进入该语句 说明用户名存在密码错误
    else if (0 != (sql_arg.mode & SQL_QUERY_PWD_ERROR)) {
        buf_send.answer = LOGON_FAIL_PWD;

        res = send(clientData->newfd, &buf_send, sizeof(buf_send), 0);
        if (0 > res) {
            perror("send_2");
            ERR_CLIENT(clientData);
        }
        PRINT_CLIENT(clientData);
        printf("密码输入有误\n");
    }

    //进入此语句表示用户名密码匹配皆成功 根据uid首位判断是管理员还是普通员工 分别进入不同的交互模式
    else {
        buf_send.answer = LOGON_OK;

        //此后该员工结构体的_pwd将有用户名的意义
        strcpy(userData._pwd, sql_arg.p_pwd);
        strcpy(buf_send.staffData.name, sql_arg.p_pwd);

        res = send(clientData->newfd, &buf_send, sizeof(buf_send), 0);
        if (0 > res) {
            perror("send_3");
            ERR_CLIENT(clientData);
        }
        PRINT_CLIENT(clientData);
        printf("登录成功\n");

        //1为：管理员
        if ('1' == userData._uid[0]) {
            while (client_root(clientData, userData)) {
                ;
            }
        }
        //0为：普通员工
        else{
            while (client_staff(clientData, userData)) {
                ;
            }
        }
    }
}


/***********************************************
 * 功能：
 *      用户登录成功后 与管理员用户交互的函数
 * 参数：
 *      clientData：客户端的信息结构体
 *      rootData：  管理员用户的账户信息
 * 返回：
 *      1：与该用户的交互未结束 上级函数会再次调用该函数
 *      0：与该用户的交互已结束 上级函数会跳出循环
 * 备注：
 *      此时的客户端UI为：
 *          1. 增加新员工
 *          2. 删除员工
 *          3. 修改员工信息
 *          4. 查询员工信息
 *          5. 查询员工的详细的访问记录
 *          6. 查看所有员工信息
 *          0. 登出
 ***********************************************/
static int client_root(ClientData *clientData, UserData rootData)
{
    int     res     =   -1;
    int     res_sql =   -1;
    Sql_arg             sql_arg;
    Msg_clientToServer  buf_recv;
    Msg_serverToClient  buf_send;

    char        buf_str_sql[SQL_STR_MAX]        = "";                                       //执行数据库查询操作的指令
    const char  str_sql_staff[SQL_STR_MAX]      = "SELECT * FROM Staff WHERE name=\"%s\";"; //管理员身份查看某员工信息  
    const char  str_sql_history[SQL_STR_MAX]    = "SELECT * FROM %s;";                      //管理员身份查询某员工的历史记录
    const char  str_sql_all[SQL_STR_MAX]        = "SELECT * FROM Staff;";                   //管理员身份查看所有员工信息

    res = recv(clientData->newfd, &buf_recv, sizeof(buf_recv), 0);
    if (0 > res) {
	    perror("recv_2");
        ERR_CLIENT(clientData);
	}
    if (0 == res) {
        EXIT_CLIENT(clientData);
    }

    switch(buf_recv.modeRoot)
    {
    //登出
    case ROOT_LOGOUT:
        PRINT_CLIENT(clientData);
        printf("用户登出\n");
        return 0;

    //添加某员工信息
    case ROOT_ADD:
        //接客户端发的注册信息包
        res = recv(clientData->newfd, &buf_recv, sizeof(buf_recv), 0);
        if (0 > res) {
            perror("recv_2_1");
        }
        if (0 == res) {
            EXIT_CLIENT(clientData);
        }

        client_register(clientData, buf_recv.userData);

        return 1;

    //删除某员工所有信息
    case ROOT_DELETE: 
        //此时的buf_recv.userData._uid为所删员工的用户名
        if (sql_staffDelete(clientData, buf_recv.userData._uid)) {
            ERR_CLIENT(clientData);
        }
        return 1;

    //修改某员工信息
    case ROOT_REVISE:
        if (sql_staffRevise(clientData, buf_recv.reviseData.reviser, buf_recv.reviseData)) {
            ERR_CLIENT(clientData);
        }
        return 1;
    
    //管理员身份查看某员工信息
    case ROOT_QUERY_STAFF: 
        memset(&sql_arg, 0, sizeof(sql_arg));
        sql_arg.mode = SQL_QUERY_STAFF | SQL_QUERY_ROOT | SQL_QUERY_SHOW_ALL;
        sql_arg.client = clientData;
        sql_arg.p_user = rootData._uid;
        sql_arg.p_pwd  = rootData._pwd;     //此时的buf_recv.userData._pwd为查询人用户名

        //此时的buf_recv.userData._uid为所查员工的用户名
        sprintf(buf_str_sql, str_sql_staff, buf_recv.userData._uid);
        res_sql = sqlite3_exec(*(clientData->db), buf_str_sql, sql_callbackQuery, &sql_arg, NULL);
        if (0 != res_sql && 1 != res_sql) {
            fprintf(stderr, "%s\n", sqlite3_errmsg(*(clientData->db)));
		    EXIT_CLIENT(clientData);
	    }
        
        //如果没有执行查询回调函数 说明没有找到数据 需要给客户端发送没有查到的消息
        if (0 == (sql_arg.mode & SQL_QUERY_YES)) {
            buf_send.answer = QUERY_NO_FIND;

            res = send(clientData->newfd, &buf_send, sizeof(buf_send), 0);
            if (0 > res) {
                perror("send_4");
                EXIT_CLIENT(clientData);
            }
            PRINT_CLIENT(clientData);
            printf("没有所需员工信息的消息已送达\n");
        }

        return 1;
    
    //管理员身份查询某员工的历史记录
    case ROOT_QUERY_STAFF_HISTORY:
        memset(&sql_arg, 0, sizeof(sql_arg));
        sql_arg.mode    = SQL_QUERY_HISTORY;
        sql_arg.client  = clientData;
        sql_arg.p_user  = rootData._uid;
        sql_arg.p_pwd   = rootData._pwd;     //此时的pwd为所查员工的用户名

        sprintf(buf_str_sql, str_sql_history, buf_recv.userData._uid);

        fprintf(stderr, "%s\n", buf_str_sql);

        res_sql = sqlite3_exec(*(clientData->db), buf_str_sql, sql_callbackQuery, &sql_arg, NULL);
        if (0 != res_sql && 1 != res_sql) {
            fprintf(stderr, "%s\n", sqlite3_errmsg(*(clientData->db)));
		    EXIT_CLIENT(clientData);
	    }
        
        //如果没有执行查询回调函数 说明没有找到数据 需要给客户端发送没有查到的消息
        if (0 == (sql_arg.mode & SQL_QUERY_YES)) {
            buf_send.answer = QUERY_NO_FIND;

            PRINT_CLIENT(clientData);
            printf("该表为空\n");
        }
        //否则 则说明历史记录列表已全部发送完毕 需要给客户端发送列举结束的消息
        else {
            buf_send.answer = QUERY_END;

            PRINT_CLIENT(clientData);
            printf("历史记录信息已发送完毕\n");
        }

        res = send(clientData->newfd, &buf_send, sizeof(buf_send), 0);
        if (0 > res) {
            perror("send_5");
            EXIT_CLIENT(clientData);
        }

        return 1;

    //管理员身份查看所有员工信息
    case ROOT_QUERY_STAFF_ALL:
        memset(&sql_arg, 0, sizeof(sql_arg));
        sql_arg.mode    = SQL_QUERY_STAFF | SQL_QUERY_ROOT | SQL_QUERY_SHOW_ALL;
        sql_arg.client  = clientData;
        sql_arg.p_user  = rootData._uid;
        sql_arg.p_pwd   = rootData._pwd; //此时的pwd为所查员工的用户名

        sprintf(buf_str_sql, "%s", str_sql_all);
        res_sql = sqlite3_exec(*(clientData->db), buf_str_sql, sql_callbackQuery, &sql_arg, NULL);
        if (0 != res_sql && 1 != res_sql) {
            fprintf(stderr, "%s\n", sqlite3_errmsg(*(clientData->db)));
		    EXIT_CLIENT(clientData);
	    }

        //如果没有执行查询回调函数 说明没有找到数据 需要给客户端发送没有查到的消息
        if (0 == (sql_arg.mode & SQL_QUERY_YES)) {
            buf_send.answer = QUERY_NO_FIND;

            PRINT_CLIENT(clientData);
            printf("该表为空\n");
        }
        //否则 则说明员工信息列表已全部发送完毕 需要给客户端发送列举结束的消息
        else {
            buf_send.answer = QUERY_END;

            PRINT_CLIENT(clientData);
            printf("历史记录信息已发送完毕\n");
        }

        res = send(clientData->newfd, &buf_send, sizeof(buf_send), 0);
        if (0 > res) {
            perror("send_5");
            EXIT_CLIENT(clientData);
        }

        return 1;


        default:
            PRINT_CLIENT(clientData);
            printf("数据解析有误\n");

            return 1;
    }

}

/***********************************************
 * 功能：
 *      用户登录成功后 与普通员工用户交互的函数
 * 参数：
 *      clientData：客户端的信息结构体
 *      staffData： 普通员工用户的账户信息
 * 返回：
 *      1：与该用户的交互未结束 上级函数会再次调用该函数
 *      0：与该用户的交互已结束 上级函数会跳出循环
 * 备注：
 *      此时的客户端UI为：
 *          1. 修改自己的信息
 *          2. 查询自己的信息\n"
 *          3. 查询自己的详细的访问记录
 *          4. 查询其他员工信息(部分信息被隐藏)
 *          5. 查看所有员工信息(部分信息被隐藏)
 *          0. 登出\n"
 ***********************************************/
static int client_staff(ClientData *clientData, UserData staffData)
{
    int     res     =   -1;
    int     res_sql =   -1;
    Sql_arg             sql_arg;
    Msg_clientToServer  buf_recv;
    Msg_serverToClient  buf_send;

    char        buf_str_sql[SQL_STR_MAX]        = "";                                       //执行数据库查询操作的指令
    const char  str_sql_staff[SQL_STR_MAX]      = "SELECT * FROM Staff WHERE name=\"%s\";"; //普通员工身份查看某员工信息  
    const char  str_sql_history[SQL_STR_MAX]    = "SELECT * FROM %s;";                      //普通员工身份查询某员工的历史记录
    const char  str_sql_all[SQL_STR_MAX]        = "SELECT * FROM Staff;";                   //普通员工身份查看所有员工信息

    res = recv(clientData->newfd, &buf_recv, sizeof(buf_recv), 0);
    if (0 > res) {
	    perror("recv");
        ERR_CLIENT(clientData);
	}
    if (0 == res) {
        EXIT_CLIENT(clientData);
    }

    fprintf(stderr, "staffname %s\n", staffData._pwd);

    switch(buf_recv.modeNormal)
    {
    //登出
    case NORMAL_LOGOUT:
        PRINT_CLIENT(clientData);
        printf("用户登出\n");
        return 0;
    
    //修改
    case NORMAL_REVISE:
        if (sql_staffRevise(clientData, buf_recv.reviseData.reviser, buf_recv.reviseData)) {
            ERR_CLIENT(clientData);
        }

        return 1;

    //查自己
    case NORMAL_QUERY_SELF:
        memset(&sql_arg, 0, sizeof(sql_arg));
        sql_arg.mode = SQL_QUERY_STAFF | SQL_QUERY_ROOT | SQL_QUERY_SHOW_ALL;
        sql_arg.client = clientData;
        sql_arg.p_user = staffData._uid;
        sql_arg.p_pwd  = staffData._pwd;     //此时的buf_recv.userData._pwd为查询人用户名

        //此时的buf_recv.userData._uid为所查员工的用户名
        sprintf(buf_str_sql, str_sql_staff, buf_recv.userData._uid);
        res_sql = sqlite3_exec(*(clientData->db), buf_str_sql, sql_callbackQuery, &sql_arg, NULL);
        if (0 != res_sql && 1 != res_sql) {
            fprintf(stderr, "%s\n", sqlite3_errmsg(*(clientData->db)));
		    EXIT_CLIENT(clientData);
	    }

        //登录成功说明该用户名一定存在 所以查询完毕后直接返回
        return 1;

    //查历史记录
    case NORMAL_QUERY_HISTORY:
        memset(&sql_arg, 0, sizeof(sql_arg));
        sql_arg.mode = SQL_QUERY_HISTORY;
        sql_arg.client = clientData;
        sql_arg.p_user = staffData._uid;
        sql_arg.p_pwd  = staffData._pwd;     //此时的pwd为所查员工的用户名

        sprintf(buf_str_sql, str_sql_history, buf_recv.userData._uid);

        fprintf(stderr, "%s\n", buf_str_sql);

        res_sql = sqlite3_exec(*(clientData->db), buf_str_sql, sql_callbackQuery, &sql_arg, NULL);
        if (0 != res_sql && 1 != res_sql) {
            fprintf(stderr, "%s\n", sqlite3_errmsg(*(clientData->db)));
		    EXIT_CLIENT(clientData);
	    }
        
        //如果没有执行查询回调函数 说明没有找到数据 需要给客户端发送没有查到的消息
        if (0 == (sql_arg.mode & SQL_QUERY_YES)) {
            buf_send.answer = QUERY_NO_FIND;

            PRINT_CLIENT(clientData);
            printf("该表为空\n");
        }
        //否则 则说明历史记录列表已全部发送完毕 需要给客户端发送列举结束的消息
        else {
            buf_send.answer = QUERY_END;

            PRINT_CLIENT(clientData);
            printf("历史记录信息已发送完毕\n");
        }

        res = send(clientData->newfd, &buf_send, sizeof(buf_send), 0);
        if (0 > res) {
            perror("send");
            EXIT_CLIENT(clientData);
        }

        return 1;

        return 1;

    //查其他
    case NORMAL_QUERY_STAFF:
        memset(&sql_arg, 0, sizeof(sql_arg));
        sql_arg.mode = SQL_QUERY_STAFF;
        sql_arg.client = clientData;
        sql_arg.p_user = staffData._uid;
        sql_arg.p_pwd  = staffData._pwd;     //此时的buf_recv.userData._pwd为查询人用户名

        //此时的buf_recv.userData._uid为所查员工的用户名
        sprintf(buf_str_sql, str_sql_staff, buf_recv.userData._uid);
        res_sql = sqlite3_exec(*(clientData->db), buf_str_sql, sql_callbackQuery, &sql_arg, NULL);
        if (0 != res_sql && 1 != res_sql) {
            fprintf(stderr, "%s\n", sqlite3_errmsg(*(clientData->db)));
		    EXIT_CLIENT(clientData);
	    }
        
        //如果没有执行查询回调函数 说明没有找到数据 需要给客户端发送没有查到的消息
        if (0 == (sql_arg.mode & SQL_QUERY_YES)) {
            buf_send.answer = QUERY_NO_FIND;

            res = send(clientData->newfd, &buf_send, sizeof(buf_send), 0);
            if (0 > res) {
                perror("send_4");
                EXIT_CLIENT(clientData);
            }
            PRINT_CLIENT(clientData);
            printf("没有所需员工信息的消息已送达\n");
        }

        return 1;

    //查所有
    case NORMAL_QUERY_STAFF_ALL:
        memset(&sql_arg, 0, sizeof(sql_arg));
        sql_arg.mode    = SQL_QUERY_STAFF;
        sql_arg.client  = clientData;
        sql_arg.p_user  = staffData._uid;
        sql_arg.p_pwd   = staffData._pwd; //此时的pwd为所查员工的用户名

        sprintf(buf_str_sql, "%s", str_sql_all);
        res_sql = sqlite3_exec(*(clientData->db), buf_str_sql, sql_callbackQuery, &sql_arg, NULL);
        if (0 != res_sql && 1 != res_sql) {
            fprintf(stderr, "%s\n", sqlite3_errmsg(*(clientData->db)));
		    EXIT_CLIENT(clientData);
	    }

        //如果没有执行查询回调函数 说明没有找到数据 需要给客户端发送没有查到的消息
        if (0 == (sql_arg.mode & SQL_QUERY_YES)) {
            buf_send.answer = QUERY_NO_FIND;

            PRINT_CLIENT(clientData);
            printf("该表为空\n");
        }
        //否则 则说明员工信息列表已全部发送完毕 需要给客户端发送列举结束的消息
        else {
            buf_send.answer = QUERY_END;

            PRINT_CLIENT(clientData);
            printf("历史记录信息已发送完毕\n");
        }

        res = send(clientData->newfd, &buf_send, sizeof(buf_send), 0);
        if (0 > res) {
            perror("send_5");
            EXIT_CLIENT(clientData);
        }

        return 1;


        default:
            PRINT_CLIENT(clientData);
            printf("数据解析有误\n");

            return 1;
    }
}
