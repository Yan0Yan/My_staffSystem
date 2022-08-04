#include "sql.h"

/***********************************************
 * 功能：
 *      打开数据库 如果没有员工表格则创建员工表格
 * 参数：
 *      db：    服务器所维护的数据库指针
 * 返回：
 *      -1：初始化数据库失败
 *       0：成功
 ***********************************************/
int sql_init(sqlite3 **db)
{
    printf("初始化数据库......\n");

    const char str[SQL_STR_MAX] = 
		"CREATE TABLE if not exists Staff ("
        "_uid char, "
        "_pwd char, "
        "name char, "
        "sex char, "
        "post char, "
        "phone char, "
        "_money char, "
        "_addr char, "
        "_birth char, "
        "_hiredata char"
    ");";

    if (sqlite3_open("./staff.db", db)) {
		ERR_SQL(*db);
	}
    printf("数据库打开成功\n");

    if (sqlite3_exec(*db, str, NULL, NULL, NULL)) {
		ERR_SQL(*db);
	}
	printf("表格Staff创建成功\n");

    printf("数据库初始化完毕\n\n");
	return 0;
}

/***********************************************
 * 功能：
 *      添加员工信息 
 *      创建以员工名为名的表格 用于记录查询记录
 *      在该查询记录查询记录下 记录创建日期和操作
 * 参数：
 *      clientData：    包含了与之交互的客户端信息和所维护的数据库指针
 *      staffData：     要添加的员工信息
 * 返回：
 *      -1：添加失败
 *       0：添加成功
 ***********************************************/
int sql_staffAdd(ClientData *clientData, StaffData staffData)
{
    //生成当前系统时间
    char now_time[HISTORY_MAX];
    get_now_time(now_time);

    char buf_str_add_table[SQL_STR_MAX]     = "";   //最终的增加表格指令语句
    char buf_str_add_staff[SQL_STR_MAX]     = "";   //最终的添加员工指令语句
    char buf_str_add_history[SQL_STR_MAX]   = "";   //最终的添加记录指令语句

    //与上面三个对应的中间语句 用于sprintf()的第二个参数
    const char str_add_table[SQL_STR_MAX]   = 
        "CREATE TABLE if not exists %s (time char, history char);";

    const char str_add_staff[SQL_STR_MAX]   = "INSERT INTO staff VALUES "
        "(\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\");";

    const char str_add_history[SQL_STR_MAX] = 
        "INSERT INTO %s VALUES (\"%s\", \"%s\");";

    //拼接生成最终的数据库指令语句
    sprintf(buf_str_add_staff, str_add_staff, 
        staffData._user._uid, 
        staffData._user._pwd, 
        staffData.name, 
        staffData.sex,
        staffData.post, 
        staffData.phone, 
        staffData._money, 
        staffData._addr,
        staffData._birth,
        staffData._hiredata
    );
    sprintf(buf_str_add_history, str_add_history,
        staffData.name,
        now_time,
        "创建该员工信息"
    );
    sprintf(buf_str_add_table, str_add_table, staffData.name);

    //将数据库操作指令发送给数据库 并在服务器终端提示 
    //(增删改的操作基本一致 以增为例 删改的函数体内不打注释)
    if (sqlite3_exec(*(clientData->db), buf_str_add_staff, NULL, NULL, NULL)) {
		ERR_SQL(*(clientData->db));
	}
    PRINT_CLIENT(clientData);
    printf("员工%s的信息插入成功\n", staffData.name);

    if (sqlite3_exec(*(clientData->db), buf_str_add_table, NULL, NULL, NULL)) {
		ERR_SQL(*(clientData->db));
	}
    PRINT_CLIENT(clientData);
    printf("员工%s的历史记录查询表格建立成功\n", staffData.name);

    if (sqlite3_exec(*(clientData->db), buf_str_add_history, NULL, NULL, NULL)) {
		ERR_SQL(*(clientData->db));
	}
    PRINT_CLIENT(clientData);
    printf("员工%s的信息的此次创建行为已被记录\n", staffData.name); 

    return 0;
}

/***********************************************
 * 功能：
 *      删除员工信息 并删除以员工名为名的历史查询记录表格
 * 参数：
 *      clientData：    包含了与之交互的客户端信息和所维护的数据库指针
 *      name：          所需删除信息的用户名
 * 返回：
 *      -1：删除中遇到致命错误
 *       0：删除成功
 ***********************************************/
int sql_staffDelete(ClientData *clientData, char name[])
{  
    Msg_serverToClient buf_send;
    char buf_str_delele_table[SQL_STR_MAX] = ""; 
    char buf_str_delele_staff[SQL_STR_MAX] = "";

    const char str_delele_table[SQL_STR_MAX] = "drop table %s;";
    const char str_delele_staff[SQL_STR_MAX] = "DELETE FROM staff WHERE name=\"%s\";";

    sprintf(buf_str_delele_table, str_delele_table, name);
    sprintf(buf_str_delele_staff, str_delele_staff, name);

    if (sqlite3_exec(*(clientData->db), buf_str_delele_staff, NULL, NULL, NULL)) {
		ERR_SQL(*(clientData->db));
	}

    if (sqlite3_exec(*(clientData->db), buf_str_delele_table, NULL, NULL, NULL)) {
        //说明要删除的员工不存在
        if (1 == sqlite3_errcode(*(clientData->db))) {
            buf_send.answer = DELETE_FAIL;
            if (0 > send(clientData->newfd, &buf_send, sizeof(buf_send), 0)) {
                ERR_CLIENT(clientData);
            }

            PRINT_CLIENT(clientData);
            printf("要删除的员工不存在\n");

            return 0;
        }

		ERR_SQL(*(clientData->db));
	}
    
    PRINT_CLIENT(clientData);
    printf("员工%s的信息删除成功\n", name);
    PRINT_CLIENT(clientData);
    printf("员工%s的历史记录查询表格删除成功\n", name);

    buf_send.answer = DELETE_OK;
    if (0 > send(clientData->newfd, &buf_send, sizeof(buf_send), 0)) {
        ERR_CLIENT(clientData);
    }

    return 0;
}

/***********************************************
 * 功能：
 *      修改员工信息 并将该修改记录保存到以操作人员为名的历史记录表格中
 * 参数：
 *      clientData：    包含了与之交互的客户端信息和所维护的数据库指针
 *      user：          修改信息人员的人员名
 *      reviseData:     包含了需要被修改的数据和要被改成的数据及其对应的数据列名和员工名
 * 返回：
 *      -1：修改失败
 *       0：修改成功
 ***********************************************/
int sql_staffRevise(ClientData *clientData, const char *user, ReviseData reviseData)
{
    Msg_serverToClient buf_send;

    char now_time[HISTORY_MAX];
    get_now_time(now_time);

    char buf_str_revise_staff[SQL_STR_MAX]  = "";
    char buf_str_add_history[SQL_STR_MAX]   = "";
    char buf_str_event[HISTORY_MAX]         = "";

    const char str_revise_staff[SQL_STR_MAX]= "UPDATE Staff SET %s=\"%s\" WHERE name=\"%s\";";
    const char str_add_history[SQL_STR_MAX] = "INSERT INTO %s VALUES (\"%s\", \"%s\");";
    const char str_event[HISTORY_MAX]       = "修改: 修改人 %s 将 %s 中改成 %s";

    sprintf(buf_str_event, str_event,
        user,
        reviseData.type,
        reviseData.newData
    );
    sprintf(buf_str_revise_staff, str_revise_staff,
        reviseData.type,
        reviseData.newData,
        reviseData.name
    );
    sprintf(buf_str_add_history, str_add_history, 
        reviseData.name,
        now_time,
        buf_str_event
    );


    if (sqlite3_exec(*(clientData->db), buf_str_revise_staff, NULL, NULL, NULL)) {
		ERR_SQL(*(clientData->db));
	}

    if (sqlite3_exec(*(clientData->db), buf_str_add_history, NULL, NULL, NULL)) {
        //说明要修改的信息输入有误
        if (1 == sqlite3_errcode(*(clientData->db))) {
            buf_send.answer = REVISE_FAIL;
            if (0 > send(clientData->newfd, &buf_send, sizeof(buf_send), 0)) {
                ERR_CLIENT(clientData);
            }

            PRINT_CLIENT(clientData);
            printf("要修改的信息输入有误\n");

            return 0;
        }

		ERR_SQL(*(clientData->db));
	}

    //运行至此 说明客户端发过来的数据包没有问题 给予回复
    buf_send.answer = REVISE_OK;
    if (0 > send(clientData->newfd, &buf_send, sizeof(buf_send), 0)) {
        ERR_CLIENT(clientData);
    }

    PRINT_CLIENT(clientData);
    printf("员工%s的信息修改成功\n", reviseData.name);
    PRINT_CLIENT(clientData);
    printf("员工%s的修改信息导入成功\n", reviseData.name);

    return 0;
}

/***********************************************
 * 功能：
 *      查询功能的回调函数 该函数会与客户端进行通信
 *      查询信息的操作如果不是被用于注册或登录 则需记录本次的查询操作
 * 参数：
 *      arg：   包含查询模式和客户端信息的结构体指针 使用时需强转成(Sql_arg*)
 *              已有对应的宏用来表示结构体的前两个成员
 *      len：   查询结果的字段数(列数)
 *      text：  该指针指向的指针数组中存着列的内容 数组为0下标存着第一列的数据 依次类推
 *      type：  该指针指向的指针数组中存着列的名字 其实就是表头
 * 返回：
 *      0： 执行成功
 * 备注：
 *      所有的模式形式(部分操作 本函数不关心查询人员的身份)：
 *          SQL_QUERY_LOGON
 *              登录操作查询用户名和密码
 *          SQL_QUERY_REGISTER                                      
 *              注册操作查询用户名
 *          SQL_QUERY_STAFF
 *              普通员工身份查看他人信息
 *          SQL_QUERY_STAFF | SQL_QUERY_SHOW_ALL
 *              普通员工身份查看自己的信息                                        
 *          SQL_QUERY_STAFF | SQL_QUERY_ROOT | SQL_QUERY_SHOW_ALL
 *              管理员身份查看员工信息
 *          SQL_QUERY_HISTORY
 *              查看历史记录信息
 *          SQL_QUERY_LOGON | SQL_QUERY_PWD_ERROR
 *              登录时密码错误(用于给上级函数的提示)
 *          SQL_QUERY_YES
 *              至少执行了一次该函数(用于给上级函数的提示)
 ***********************************************/
int sql_callbackQuery(void *arg, int len, char **text, char **type)
{
    int                 res = -1;
    Msg_serverToClient  buf_send;
    memset(&buf_send, 0, sizeof(buf_send));

    //防止部分可能会执行两次该函数的情况下不能执行
    SQL_ARG_MODE &= ~SQL_QUERY_YES;

    PRINT_CLIENT(SQL_ARG_CLIENT);
    fprintf(stderr, "mode = %d\n", SQL_ARG_MODE);

    //登录模式下 运行到这表示uid匹配成功
    if (SQL_QUERY_LOGON == SQL_ARG_MODE) {
        //如果不相等说明密码有误
        if (0 != strcmp(((Sql_arg*)arg)->p_pwd, text[1]) ) {
            SQL_ARG_MODE |= SQL_QUERY_PWD_ERROR;
        }
        //将arg中的_pwd改成用户名
        strcpy(((Sql_arg*)arg)->p_pwd, text[2]);
    }

    //注册模式下 运行到这说明已有该uid 注册失败
    else if (SQL_QUERY_REGISTER == SQL_ARG_MODE) {
        SQL_ARG_MODE |= SQL_QUERY_YES;
    }

    //普通员工查询其他员工信息 以"_"起头的皆为隐藏信息 以"******"的形式对其发送
    else if (SQL_QUERY_STAFF == SQL_ARG_MODE) {
        //拼接协议 发送给客户端
        buf_send.answer = QUERY_OK;

        sprintf(buf_send.staffData._user._uid,  "%s", "******");
        sprintf(buf_send.staffData._user._pwd,  "%s", "******");
        sprintf(buf_send.staffData.name,        "%s", text[2]);
        sprintf(buf_send.staffData.sex,         "%s", text[3]);
        sprintf(buf_send.staffData.post,        "%s", text[4]);
        sprintf(buf_send.staffData.phone,       "%s", text[5]);
        sprintf(buf_send.staffData._addr,       "%s", "******");
        sprintf(buf_send.staffData._birth,      "%s", "******");
        sprintf(buf_send.staffData._hiredata,   "%s", "******");
        sprintf(buf_send.staffData._money,      "%s", "******");

        res = send(SQL_ARG_CLIENT->newfd, &buf_send, sizeof(buf_send), 0);
		if (0 > res) {
			perror("send_sql_1");
            ERR_CLIENT(SQL_ARG_CLIENT);
		}

        //拼接数据库指令 添加本次查询记录
        char now_time[HISTORY_MAX];
        get_now_time(now_time);

        char buf_str_history[HISTORY_MAX]    = "";
        char buf_str_event[HISTORY_MAX]      = "";

        const char str_history[HISTORY_MAX]  = "INSERT INTO %s VALUES (\"%s\", \"%s\");";
        const char str_event[HISTORY_MAX]    = "查询了 %s 的员工信息";

        sprintf(buf_str_event, str_event, buf_send.staffData.name);
        sprintf(buf_str_history, str_history,
            ((Sql_arg*)arg)->p_pwd,     //p_pwd为所查员工的用户名
            now_time,
            buf_str_event
        );

        if (sqlite3_exec(*(SQL_ARG_CLIENT->db), buf_str_history, NULL, NULL, NULL)) {
		    ERR_SQL(*(SQL_ARG_CLIENT->db));
	    }
        PRINT_CLIENT(SQL_ARG_CLIENT);
        printf("查询人员%s的查询记录已导入\n", ((Sql_arg*)arg)->p_pwd);
    }

    //普通员工查询自己的信息 或者 管理员查看员工的信息
    else if ((SQL_QUERY_STAFF | SQL_QUERY_SHOW_ALL) == SQL_ARG_MODE || 
            (SQL_QUERY_STAFF | SQL_QUERY_ROOT | SQL_QUERY_SHOW_ALL) == SQL_ARG_MODE) {
        //拼接协议 发送给客户端
        buf_send.answer = QUERY_OK;

        sprintf(buf_send.staffData._user._uid,  "%s", text[0]);
        sprintf(buf_send.staffData._user._pwd,  "%s", text[1]);
        sprintf(buf_send.staffData.name,        "%s", text[2]);
        sprintf(buf_send.staffData.sex,         "%s", text[3]);
        sprintf(buf_send.staffData.post,        "%s", text[4]);
        sprintf(buf_send.staffData.phone,       "%s", text[5]);
        sprintf(buf_send.staffData._addr,       "%s", text[6]);
        sprintf(buf_send.staffData._birth,      "%s", text[7]);
        sprintf(buf_send.staffData._hiredata,   "%s", text[8]);
        sprintf(buf_send.staffData._money,      "%s", text[9]);

        res = send(SQL_ARG_CLIENT->newfd, &buf_send, sizeof(buf_send), 0);
		if (0 > res) {
			perror("send_sql_2");
            ERR_CLIENT(SQL_ARG_CLIENT);
		}

        //拼接数据库指令 添加本次查询记录
        char now_time[HISTORY_MAX];
        get_now_time(now_time);

        char buf_str_history[HISTORY_MAX]    = "";
        char buf_str_event[HISTORY_MAX]      = "";

        const char str_history[HISTORY_MAX]  = "INSERT INTO %s VALUES (\"%s\", \"%s\");";
        const char str_event[HISTORY_MAX]    = "查询了 %s 的员工信息";

        sprintf(buf_str_event, str_event, buf_send.staffData.name);
        sprintf(buf_str_history, str_history,
            ((Sql_arg*)arg)->p_pwd,     //p_pwd为所查员工的用户名
            now_time,
            buf_str_event
        );

        fprintf(stderr, "cha: %s \n", buf_str_history);

        if (sqlite3_exec(*(SQL_ARG_CLIENT->db), buf_str_history, NULL, NULL, NULL)) {
		    ERR_SQL(*(SQL_ARG_CLIENT->db));
	    }
        PRINT_CLIENT(SQL_ARG_CLIENT);
        printf("查询人员%s的查询记录已导入\n", ((Sql_arg*)arg)->p_pwd);
    }

    //查询历史记录 
    else if (SQL_QUERY_HISTORY == SQL_ARG_MODE) {
        buf_send.answer = QUERY_OK;

        sprintf(buf_send.historyData.time,      "%s", text[0]);
        sprintf(buf_send.historyData.history,   "%s", text[1]);

        res = send(SQL_ARG_CLIENT->newfd, &buf_send, sizeof(buf_send), 0);
		if (0 > res) {
			perror("send_sql_3");
            ERR_CLIENT(SQL_ARG_CLIENT);
		}
    }

    //能运行这表示至少执行过一次该回调函数 上级函数通过此可以判断至少有一次数据被查询到
    SQL_ARG_MODE |= SQL_QUERY_YES;

    return 0;
}

/***********************************************
 * 功能：
 *      获取当前系统时间
 * 参数：
 *      now_time：  获取的系统时间转化成字符串给它
 * 返回：
 *      void
 ***********************************************/
static void get_now_time(char now_time[])
{
    time_t t;
    struct tm *now = NULL;

    time(&t); 
    now = localtime(&t);

    sprintf(now_time, "%04d年%02d月%02d日 %02d:%02d:%02d",
        now->tm_year + 1900,
        now->tm_mon + 1,
        now->tm_mday,
        now->tm_hour,
        now->tm_min,
        now->tm_sec
    );
}