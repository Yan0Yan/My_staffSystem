#include "client.h"

/***********************************************
 * 功能：
 *      与服务器建立连接
 * 参数：
 *      sfd：       要创建的套接字的文件描述符指针
 *      sin：       服务器地址信息的结构体指针
 *      addr_len：  服务器地址信息的结构体大小
 * 返回：
 *      0： 与服务器成功建立连接
 *     -1： 与服务器建立连接失败
 ***********************************************/
int connect_server(int *sfd, struct sockaddr_in *sin, int addr_len)
{
    *sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
        perror("socket");
        return -1;
    }
    printf("成功创建流式套接字\n");

    sin->sin_family         = AF_INET;
    sin->sin_port           = htons(SERVER_PORT);
    sin->sin_addr.s_addr    = inet_addr(SERVER_IP);
    if (connect(*sfd, (struct sockaddr*)sin, addr_len)) {
        perror("connect");
        return -1;
    }
    printf("连接服务器成功\n");

    return 0;
 }

/***********************************************
 * 功能：
 *      登录之前与用户交互的函数
 * 参数：
 *      sfd：   套接字文件描述符
 * 返回：
 *      0:      用户主动退出程序
 *      1：     回到上级函数中 上级函数会再次调用该函数 与用户互动
 * 备注：
 *      此时客户端进入init_welcome() 界面为：
 *          1. 登录
 *          2. 注册
 *          0. 退出
 ***********************************************/
int user_init(int sfd)
{
    char    mode    = 0;

    scanf("%c", &mode);
    while ('\n' != getchar()) {
        ;
    }
    //如果用户输入的数字不在0-2范围 则退出该函数 上级函数会重新调用该函数
    if (mode < '0' || mode > '2') {
        printf("输入错误 请输入0-2中的一个数字 按回车键继续\n");
        while ('\n' != getchar()) {
            ;
        }
        return 1;
    }

    switch (mode)
    {
    case INIT_LOGOUT:
        printf("感谢您的使用 再见! \n");
        return 0;

    case INIT_LOGON:
        user_logon(sfd);
        return 1;

    case INIT_REGISTER:
        user_register(sfd);
        return 1;
    }
}

/***********************************************
 * 功能：
 *      处理用户登录函数
 * 参数：
 *      sfd：   套接字文件描述符
 * 返回：
 *      void
 ***********************************************/
static void user_logon(int sfd)
{
    int res = -1;
    Msg_clientToServer buf_send;
    Msg_serverToClient buf_recv;

    buf_send.modeInit = INIT_LOGON;
    printf("请输入用户名：\n");
    fgets(buf_send.userData._uid, STAFF_MAX, stdin);
    buf_send.userData._uid[strlen(buf_send.userData._uid) - 1] = 0;
    printf("请输入密码：\n");
    fgets(buf_send.userData._pwd, STAFF_MAX, stdin);
    buf_send.userData._pwd[strlen(buf_send.userData._pwd) - 1] = 0;

    //给服务器发登录信息包 
    res = send(sfd, &buf_send, sizeof(buf_send), 0);
    if (0 > res) {
        ERR_CLIENT_CLOSE("send_1");
    }

    //服务器给予登录情况的回应
    res = recv(sfd, &buf_recv,sizeof(buf_recv), 0);
    if (0 > res) {
        ERR_CLIENT_CLOSE("recv_1");
    }
    if (0 == res) {
        SERVER_CLOSE;
    }

    //解析服务器传递的数据包
    if (LOGON_FAIL_UID == buf_recv.answer) {
        printf("用户名不存在 按回车继续\n");
        while ('\n' != getchar()) {
            ;
        }
    }
    else if (LOGON_FAIL_PWD == buf_recv.answer) {
        printf("密码错误 按回车继续\n");
        while ('\n' != getchar()) {
            ;
        }
    }
    
    else if (LOGON_OK == buf_recv.answer) {
        //根据用户uid的首位 进入不同的界面 1管理员 0普通员工
        if ('1' == buf_send.userData._uid[0]) {
            do {
                welcome_root(buf_recv.staffData.name);
            } while (root_logon(sfd, buf_recv.staffData.name));
        }
        else {
            do {
                welcome_normal(buf_recv.staffData.name);
            } while (staff_logon(sfd, buf_recv.staffData.name));
        }
    }
}

/***********************************************
 * 功能：
 *      处理用户注册函数
 * 参数：
 *      sfd：   套接字文件描述符
 * 返回：
 *      void
 ***********************************************/
static void user_register(int sfd)
{
    int     res     =   -1;
    Msg_clientToServer  buf_send;
    Msg_serverToClient  buf_recv;

    printf(
        "注册前须知：\n"
        "1. uid为6位数纯数字\n"
        "2. uid首位只能是0或1 0为普通用户注册 1表示管理员注册\n"
        "3. 密码为5-20位\n"
        "4. 注册管理员需要额外输入一次特殊密码才可以成功注册\n"
        "5. 务必遵守注册规则 谢谢!\n\n"
    );

    printf("请输入uid: \n");
    scanf("%s", buf_send.userData._uid);
    while ('\n' != getchar()) {
        ;
    }

    //如果用户首位输入的数字不是0或1 操作按位与0B11111110会使其得非0
    if (0 != ((buf_send.userData._uid[0] - 48) & 0xFE) ||
        6 != strlen(buf_send.userData._uid) ||
        strspn(buf_send.userData._uid, "1234567890") != strlen(buf_send.userData._uid)
    ) {
        printf("用户名格式非法! 按回车继续\n");
        while ('\n' != getchar()) {
            ;
        }
        return;
    }

    printf("请输入密码: \n");
    scanf("%s", buf_send.userData._pwd);
    while ('\n' != getchar()) {
        ;
    }

    if (strlen(buf_send.userData._pwd) < 5 ||
        strlen(buf_send.userData._pwd) > 20
    ) {
        printf("密码格式非法! 按回车继续\n");
        while ('\n' != getchar()) {
            ;
        }
        return;
    }

    if ('1' == buf_send.userData._uid[0]) {
        char super_str[20] = "";

        printf("您使用了管理员身份进行注册 必须输入超级密码才能注册成功: \n");
        scanf("%s", super_str);
        while ('\n' != getchar()) {
            ;
        }

        if (strcmp(super_str, SUPER_PWD)) {
            printf("超级密码不正确! 按回车键继续\n");
            while ('\n' != getchar()) {
                ;
            }
            return;
        }

        printf("超级密码输入正确!\n");
    }

    //注册格式正确之后 给服务器发送该包
    buf_send.modeInit = INIT_REGISTER;
    res = send(sfd, &buf_send, sizeof(buf_send), 0);
    if (0 > res) {
        ERR_CLIENT_CLOSE("send_2");
    }

    //再接服务器的回应包 确认数据库是否入库成功
    res = recv(sfd, &buf_recv, sizeof(buf_recv), 0);
    if (0 > res) {
        ERR_CLIENT_CLOSE("recv_2");
    }
    if (0 == res) {
        SERVER_CLOSE;
    }

    if (REGISTER_FAIL_UID == buf_recv.answer) {
        printf("抱歉 该用户名已存在 注册失败 按回车继续\n");
        while ('\n' != getchar()) {
            ;
        }
        return;
    }
    else if (REGISTER_OK == buf_recv.answer) {
        printf("注册成功!\n");
    }

    //进入添加员工信息函数
    root_add_staff(sfd, buf_send.userData);
    printf("按回车继续\n");
    while ('\n' != getchar()) {
        ;
    }
}

/***********************************************
 * 功能：
 *      与登录成功后的管理员用户交互
 * 参数：
 *      sfd：       套接字文件描述符
 *      username：  该用户的用户名
 * 返回：
 *      0：     表示用户登出
 *      1：     继续与该用户进行交互
 * 备注：   
 *      此时的客户端界面
 *          1. 增加新员工
 *          2. 删除员工
 *          3. 修改员工信息
 *          4. 查询员工信息
 *          5. 查询员工的详细的访问记录
 *          6. 查看所有员工信息
 *          0. 登出
 ***********************************************/
static int root_logon(int sfd, char username[])
{
    int         res     = -1; 
    char        mode    =  0;
    Msg_clientToServer  buf_send;

    scanf("%c", &mode);
    while ('\n' != getchar()) {
        ;
    }
    //如果用户输入的数字不在0-6范围 则退出该函数 上级函数会重新调用该函数
    if (mode < '0' || mode > '6') {
        printf("输入错误 请输入0-6中的一个数字 按回车键继续\n");
        while ('\n' != getchar()) {
            ;
        }
        return 1;
    }

    switch (mode)
    {
    case ROOT_LOGOUT:
        buf_send.modeRoot = ROOT_LOGOUT;
        res = send(sfd, &buf_send, sizeof(buf_send), 0);
        if (0 > res) {
            ERR_CLIENT_CLOSE("send_5");
        }

        printf("再见 %s ! 按回车继续\n", username);
        while ('\n' != getchar()) {
            ;
        }
        return 0;

    case ROOT_ADD:
        //如果是管理员添加新员工 则相当于走一遍注册流程
        buf_send.modeRoot = ROOT_ADD;
        res = send(sfd, &buf_send, sizeof(buf_send), 0);
        if (0 > res) {
            ERR_CLIENT_CLOSE("send_5");
        }

        user_register(sfd);
        return 1;

    case ROOT_DELETE:
        root_delete_staff(sfd);
        return 1;

    case ROOT_REVISE:
        root_revise_staff(sfd, username);
        return 1;

    case ROOT_QUERY_STAFF:
        root_query_staff(sfd, username);
        return 1;

    case ROOT_QUERY_STAFF_HISTORY:
        root_query_history(sfd, username);
        return 1;

    case ROOT_QUERY_STAFF_ALL:
        root_query_all(sfd, username);
        return 1;
    }
}

/***********************************************
 * 功能：
 *      与登录成功后的普通员工用户交互
 * 参数：
 *      sfd：       套接字文件描述符
 *      username：  该用户的用户名
 * 返回：
 *      0：     表示用户登出
 *      1：     继续与该用户进行交互
 * 备注：
 *      此时的客户端界面：
 *          1. 修改自己的信息
 *          2. 查询自己的信息
 *          3. 查询自己的详细的访问记录
 *          4. 查询其他员工信息(部分信息被隐藏)
 *          5. 查看所有员工信息(部分信息被隐藏)
 *          0. 登出
 ***********************************************/
static int staff_logon(int sfd, char username[])
{
    int         res     = -1; 
    char        mode    =  0;
    Msg_clientToServer  buf_send;

    scanf("%c", &mode);
    while ('\n' != getchar()) {
        ;
    }
    //如果用户输入的数字不在0-6范围 则退出该函数 上级函数会重新调用该函数
    if (mode < '0' || mode > '6') {
        printf("输入错误 请输入0-6中的一个数字 按回车键继续\n");
        while ('\n' != getchar()) {
            ;
        }
        return 1;
    }

    switch (mode)
    {
    case NORMAL_LOGOUT:
        buf_send.modeNormal = NORMAL_LOGOUT;
        res = send(sfd, &buf_send, sizeof(buf_send), 0);
        if (0 > res) {
            ERR_CLIENT_CLOSE("send");
        }

        printf("再见 %s ! 按回车继续\n", username);
        while ('\n' != getchar()) {
            ;
        }
        return 0;

    case NORMAL_REVISE:
        normal_revise_staff(sfd, username);

        return 1;

    case NORMAL_QUERY_SELF:
        normal_query_self(sfd, username);

        return 1;

    case NORMAL_QUERY_HISTORY:
        normal_query_history(sfd, username);

        return 1;

    case NORMAL_QUERY_STAFF:
        normal_query_staff(sfd, username);

        return 1;

    case NORMAL_QUERY_STAFF_ALL:
        normal_query_all(sfd, username);

        return 1;
    }
}

/***********************************************
 * 功能：
 *      将要添加的新员工信息发送给服务器 服务器将信息入库
 * 参数：
 *      sfd：       套接字文件描述符
 *      userData:   要添加的员工信息的uid和密码
 * 返回：
 *      void         
 ***********************************************/
static void root_add_staff(int sfd, UserData userData)
{   
    int     res             = -1;
    Msg_clientToServer      buf_send;

    buf_send.modeRoot = ROOT_ADD;
    buf_send.staffData._user = userData;

    printf("姓名\n");
    fgets(buf_send.staffData.name, sizeof(buf_send.staffData.name), stdin);
    buf_send.staffData.name[strlen(buf_send.staffData.name) - 1] = 0;

    printf("性别\n");
    fgets(buf_send.staffData.sex, sizeof(buf_send.staffData.sex), stdin);
    buf_send.staffData.sex[strlen(buf_send.staffData.sex) - 1] = 0;

    printf("职位\n");
    fgets(buf_send.staffData.post, sizeof(buf_send.staffData.post), stdin);
    buf_send.staffData.post[strlen(buf_send.staffData.post) - 1] = 0;

    printf("电话\n");
    fgets(buf_send.staffData.phone, sizeof(buf_send.staffData.phone), stdin);
    buf_send.staffData.phone[strlen(buf_send.staffData.phone) - 1] = 0;

    printf("月薪\n");
    fgets(buf_send.staffData._money, sizeof(buf_send.staffData._money), stdin);
    buf_send.staffData._money[strlen(buf_send.staffData._money) - 1] = 0;

    printf("住址\n");
    fgets(buf_send.staffData._addr, sizeof(buf_send.staffData._addr), stdin);
    buf_send.staffData._addr[strlen(buf_send.staffData._addr) - 1] = 0;

    printf("出生日期\n");
    fgets(buf_send.staffData._birth, sizeof(buf_send.staffData._birth), stdin);
    buf_send.staffData._birth[strlen(buf_send.staffData._birth) - 1] = 0;

    printf("入职日期\n");
    fgets(buf_send.staffData._hiredata, sizeof(buf_send.staffData._hiredata), stdin);
    buf_send.staffData._hiredata[strlen(buf_send.staffData._hiredata) - 1] = 0;

    //发包
    res = send(sfd, &buf_send, sizeof(buf_send), 0);
    if (0 > res) {
        ERR_CLIENT_CLOSE("send_3");
    }
    
    printf("员工信息导入成功\n");
}

/***********************************************
 * 功能：
 *      删除某员工的所有信息
 * 参数：
 *      sfd：   套接字文件描述符
 * 返回：
 *      void
 ***********************************************/
static void root_delete_staff(int sfd)
{
    int     res             = -1;
    Msg_clientToServer      buf_send;
    Msg_serverToClient      buf_recv;

    printf("请输入需要删除的员工名\n");
    fgets(buf_send.userData._uid, sizeof(buf_send.userData._uid), stdin);
    buf_send.userData._uid[strlen(buf_send.userData._uid) - 1] = 0;

    buf_send.modeRoot = ROOT_DELETE;

    res = send(sfd, &buf_send, sizeof(buf_send), 0);
    if (0 > res) {
        ERR_CLIENT_CLOSE("send_4");
    }

    res = recv(sfd, &buf_recv, sizeof(buf_recv), 0);
    if (0 > res) {
        ERR_CLIENT_CLOSE("recv_4");
    }
    if (0 == res) {
        SERVER_CLOSE;
    }

    if (DELETE_FAIL == buf_recv.answer) {
        printf("删除失败 不存在这个员工 按任意键继续\n");
    }
    else if (DELETE_OK == buf_recv.answer) {
        printf("员工信息删除成功! 按任意键继续\n");
    }

    while ('\n' != getchar()) {
        ;
    }
}

/***********************************************
 * 功能：
 *      修改某员工的某个信息
 * 参数：
 *      sfd：       套接字文件描述符
 *      username:   管理员名
 * 返回：
 *      void
 ***********************************************/
static void root_revise_staff(int sfd, char username[])
{
    int     res             = -1;
    int     user_input      = -1;
    Msg_clientToServer      buf_send;
    Msg_serverToClient      buf_recv;
    char arr_type[8][STAFF_MAX] = {
        "_pwd", "sex", "post", "phone", "_money", "_addr", "_birth", "_hiredata"
    };

    printf("请输入需要更改的员工名\n");
    fgets(buf_send.reviseData.name, sizeof(buf_send.reviseData.name), stdin);
    buf_send.reviseData.name[strlen(buf_send.reviseData.name) - 1] = 0;

    printf("\n请输入要更改的类型: (输入0-9中的数) (uid和员工名不可更改)\n");
    for (int i = 0; i < 8; i++) {
        printf("%d. %s\n", i, arr_type[i]);
    }

    scanf("%d", &user_input);
    while ('\n' != getchar()) {
        ;
    }

    if (0 > user_input || 9 < user_input) {
        printf("输入有误! 按回车继续\n");
        while ('\n' != getchar()) {
            ;
        }
        return;
    }
    sprintf(buf_send.reviseData.type, "%s", arr_type[user_input]);

    printf("请输入新数据\n");
    fgets(buf_send.reviseData.newData, sizeof(buf_send.reviseData.newData), stdin);
    buf_send.reviseData.newData[strlen(buf_send.reviseData.newData) - 1] = 0;

    buf_send.modeRoot = ROOT_REVISE;
    strcpy(buf_send.reviseData.reviser, username);

    res = send(sfd, &buf_send, sizeof(buf_send), 0);
    if (0 > res) {
        ERR_CLIENT_CLOSE("send");
    }

    res = recv(sfd, &buf_recv, sizeof(buf_recv), 0);
    if (0 > res) {
        ERR_CLIENT_CLOSE("recv");
    }
    if (0 == res) {
        SERVER_CLOSE;
    }

    //解析服务器的回复 判断是否修改成功
    if (REVISE_FAIL == buf_recv.answer) {
        printf("输入的数据有误 修改失败 按下回车继续\n");
    }
    else if (REVISE_OK == buf_recv.answer) {
        printf("数据修改成功 按下回车继续\n");
    }

    while ('\n' != getchar()) {
        ;
    }
}

/***********************************************
 * 功能：
 *      管理员：查询某员工的信息
 * 参数：
 *      sfd：       套接字文件描述符
 *      username:   管理员名
 * 返回：
 *      void
 ***********************************************/
static void root_query_staff(int sfd, char username[])
{
    int     res             = -1;
    Msg_clientToServer      buf_send;
    Msg_serverToClient      buf_recv;

    buf_send.modeRoot = ROOT_QUERY_STAFF;

    printf("请输入要查看的员工名\n");
    fgets(buf_send.userData._uid, sizeof(buf_send.userData._uid), stdin);
    buf_send.userData._uid[strlen(buf_send.userData._uid) - 1] = 0;
    strcpy(buf_send.userData._pwd, username);   //此时的_pwd充当查询人员用户名

    res = send(sfd, &buf_send, sizeof(buf_send), 0);
    if (0 > res) {
        ERR_CLIENT_CLOSE("send");
    }

    res = recv(sfd, &buf_recv, sizeof(buf_recv), 0);
    if (0 > res) {
        ERR_CLIENT_CLOSE("recv");
    }
    if (0 == res) {
        SERVER_CLOSE;
    }

    //解析服务器的回复 判断是否查询成功
    if (QUERY_NO_FIND == buf_recv.answer) {
        printf("输入的数据有误 查询失败 按下回车继续\n");
    }
    else if (QUERY_OK == buf_recv.answer) {
        printf("uid      : %s\n", buf_recv.staffData._user._uid);
        printf("pwd      : %s\n", buf_recv.staffData._user._pwd);
        printf("name     : %s\n", buf_recv.staffData.name);
        printf("phone    : %s\n", buf_recv.staffData.phone);
        printf("post     : %s\n", buf_recv.staffData.post);
        printf("sex      : %s\n", buf_recv.staffData.sex);
        printf("addr     : %s\n", buf_recv.staffData._addr);
        printf("birth    : %s\n", buf_recv.staffData._birth);
        printf("hiredata : %s\n", buf_recv.staffData._hiredata);
        printf("money    : %s\n", buf_recv.staffData._money);

        printf("\n该员工信息如上 按回车继续\n");
    }

    while ('\n' != getchar()) {
        ;
    }
}

/***********************************************
 * 功能：
 *      管理员：查询某员工的历史记录
 * 参数：
 *      sfd：       套接字文件描述符
 *      username:   管理员名
 * 返回：
 *      void
 ***********************************************/
static void root_query_history(int sfd, char username[])
{
    int     res             = -1;
    Msg_clientToServer      buf_send;
    Msg_serverToClient      buf_recv;

    buf_send.modeRoot = ROOT_QUERY_STAFF_HISTORY;

    printf("请输入要查看的员工名\n");
    fgets(buf_send.userData._uid, sizeof(buf_send.userData._uid), stdin);
    buf_send.userData._uid[strlen(buf_send.userData._uid) - 1] = 0;
    strcpy(buf_send.userData._pwd, username);   //此时的_pwd充当查询人员用户名

    res = send(sfd, &buf_send, sizeof(buf_send), 0);
    if (0 > res) {
        ERR_CLIENT_CLOSE("send");
    }

    while (1) {
        res = recv(sfd, &buf_recv, sizeof(buf_recv), 0);
        if (0 > res) {
            ERR_CLIENT_CLOSE("recv");
        }
        if (0 == res) {
            SERVER_CLOSE;
        }

        //查到数据的情况
        if (QUERY_OK == buf_recv.answer) {
            printf("%s : %s\n", buf_recv.historyData.time, buf_recv.historyData.history);
        }
        //数据已列完的情况
        if (QUERY_END == buf_recv.answer) {
            printf("\n历史记录已列完 请按回车继续\n");
            break;
        }
        //数据为空的情况
        if (QUERY_NO_FIND == buf_recv.answer) {
            printf("输入的数据有误 请按回车继续n");
            break;
        }
    }

    while ('\n' != getchar()) {
        ;
    }
}

/***********************************************
 * 功能：
 *      管理员：查询所有员工的信息
 * 参数：
 *      sfd：       套接字文件描述符
 *      username:   管理员名
 * 返回：
 *      void
 ***********************************************/
static void root_query_all(int sfd, char username[])
{
    int     res             = -1;
    Msg_clientToServer      buf_send;
    Msg_serverToClient      buf_recv;

    buf_send.modeRoot = ROOT_QUERY_STAFF_ALL;
    strcpy(buf_send.userData._pwd, username);   //此时的_pwd充当查询人员用户名

    res = send(sfd, &buf_send, sizeof(buf_send), 0);
    if (0 > res) {
        ERR_CLIENT_CLOSE("send");
    }

    while (1) {
        res = recv(sfd, &buf_recv, sizeof(buf_recv), 0);
        if (0 > res) {
            ERR_CLIENT_CLOSE("recv");
        }
        if (0 == res) {
            SERVER_CLOSE;
        }

        //查到数据的情况
        if (QUERY_OK == buf_recv.answer) {
            printf("\n");
            printf("uid      : %s\n", buf_recv.staffData._user._uid);
            printf("pwd      : %s\n", buf_recv.staffData._user._pwd);
            printf("name     : %s\n", buf_recv.staffData.name);
            printf("phone    : %s\n", buf_recv.staffData.phone);
            printf("post     : %s\n", buf_recv.staffData.post);
            printf("sex      : %s\n", buf_recv.staffData.sex);
            printf("addr     : %s\n", buf_recv.staffData._addr);
            printf("birth    : %s\n", buf_recv.staffData._birth);
            printf("hiredata : %s\n", buf_recv.staffData._hiredata);
            printf("money    : %s\n", buf_recv.staffData._money);
        }
        //数据已列完的情况
        if (QUERY_END == buf_recv.answer) {
            printf("\n所有员工信息已列完 请按回车继续\n");
            break;
        }
        //数据为空的情况
        if (QUERY_NO_FIND == buf_recv.answer) {
            printf("输入的数据有误 请按回车继续n");
            break;
        }
    }

    while ('\n' != getchar()) {
        ;
    }
}

/***********************************************
 * 功能：
 *      修改自己的某个信息
 * 参数：
 *      sfd：       套接字文件描述符
 *      username:   普通员工名
 * 返回：
 *      void
 ***********************************************/
static void normal_revise_staff(int sfd, char username[])
{
    int     res             = -1;
    int     user_input      = -1;
    Msg_clientToServer      buf_send;
    Msg_serverToClient      buf_recv;
    const char arr_type[8][STAFF_MAX] = {
        "_pwd", "sex", "post", "phone", "_money", "_addr", "_birth", "_hiredata"
    };

    printf("\n请输入要更改的类型: (输入0-9中的数) (uid和员工名不可更改)\n");
    for (int i = 0; i < 8; i++) {
        printf("%d. %s\n", i, arr_type[i]);
    }

    scanf("%d", &user_input);
    while ('\n' != getchar()) {
        ;
    }

    if (0 > user_input || 9 < user_input) {
        printf("输入有误! 按回车继续\n");
        while ('\n' != getchar()) {
            ;
        }
        return;
    }
    sprintf(buf_send.reviseData.type, "%s", arr_type[user_input]);

    printf("请输入新数据\n");
    fgets(buf_send.reviseData.newData, sizeof(buf_send.reviseData.newData), stdin);
    buf_send.reviseData.newData[strlen(buf_send.reviseData.newData) - 1] = 0;

    buf_send.modeNormal = NORMAL_REVISE;
    strcpy(buf_send.reviseData.reviser, username);
    strcpy(buf_send.reviseData.name, username);

    res = send(sfd, &buf_send, sizeof(buf_send), 0);
    if (0 > res) {
        ERR_CLIENT_CLOSE("send");
    }

    res = recv(sfd, &buf_recv, sizeof(buf_recv), 0);
    if (0 > res) {
        ERR_CLIENT_CLOSE("recv");
    }
    if (0 == res) {
        SERVER_CLOSE;
    }

    //解析服务器的回复 判断是否修改成功
    if (REVISE_FAIL == buf_recv.answer) {
        printf("输入的数据有误 修改失败 按下回车继续\n");
    }
    else if (REVISE_OK == buf_recv.answer) {
        printf("数据修改成功 按下回车继续\n");
    }

    while ('\n' != getchar()) {
        ;
    }
}

/***********************************************
 * 功能：
 *      普通员工：查询自己的员工的信息
 * 参数：
 *      sfd：       套接字文件描述符
 *      username:   普通员工名
 * 返回：
 *      void
 ***********************************************/
static void normal_query_self(int sfd, char username[])
{
    int     res             = -1;
    Msg_clientToServer      buf_send;
    Msg_serverToClient      buf_recv;

    buf_send.modeNormal = NORMAL_QUERY_SELF;
    sprintf(buf_send.userData._uid, "%s", username);
    sprintf(buf_send.userData._pwd, "%s", username);    //此时的pwd为查询人员 也就是自己

    res = send(sfd, &buf_send, sizeof(buf_send), 0);
    if (0 > res) {
        ERR_CLIENT_CLOSE("send");
    }

    res = recv(sfd, &buf_recv, sizeof(buf_recv), 0);
    if (0 > res) {
        ERR_CLIENT_CLOSE("recv");
    }
    if (0 == res) {
        SERVER_CLOSE;
    }

    printf("\n");
    printf("uid      : %s\n", buf_recv.staffData._user._uid);
    printf("pwd      : %s\n", buf_recv.staffData._user._pwd);
    printf("name     : %s\n", buf_recv.staffData.name);
    printf("phone    : %s\n", buf_recv.staffData.phone);
    printf("post     : %s\n", buf_recv.staffData.post);
    printf("sex      : %s\n", buf_recv.staffData.sex);
    printf("addr     : %s\n", buf_recv.staffData._addr);
    printf("birth    : %s\n", buf_recv.staffData._birth);
    printf("hiredata : %s\n", buf_recv.staffData._hiredata);
    printf("money    : %s\n", buf_recv.staffData._money);

    printf("您的信息如上 请按回车继续\n");
    while ('\n' != getchar()) {
        ;
    }
}

/***********************************************
 * 功能：
 *      普通员工：查询别的员工的信息
 * 参数：
 *      sfd：       套接字文件描述符
 *      username:   普通员工名
 * 返回：
 *      void  
 ***********************************************/
static void normal_query_staff(int sfd, char username[])
{
    int     res             = -1;
    Msg_clientToServer      buf_send;
    Msg_serverToClient      buf_recv;

    buf_send.modeRoot = NORMAL_QUERY_STAFF;

    printf("请输入要查看的员工名\n");
    fgets(buf_send.userData._uid, sizeof(buf_send.userData._uid), stdin);
    buf_send.userData._uid[strlen(buf_send.userData._uid) - 1] = 0;
    strcpy(buf_send.userData._pwd, username);   //此时的_pwd充当查询人员用户名

    res = send(sfd, &buf_send, sizeof(buf_send), 0);
    if (0 > res) {
        ERR_CLIENT_CLOSE("send");
    }

    res = recv(sfd, &buf_recv, sizeof(buf_recv), 0);
    if (0 > res) {
        ERR_CLIENT_CLOSE("recv");
    }
    if (0 == res) {
        SERVER_CLOSE;
    }

    //解析服务器的回复 判断是否查询成功
    if (QUERY_NO_FIND == buf_recv.answer) {
        printf("输入的数据有误 查询失败 按下回车继续\n");
    }
    else if (QUERY_OK == buf_recv.answer) {
        printf("uid      : %s\n", buf_recv.staffData._user._uid);
        printf("pwd      : %s\n", buf_recv.staffData._user._pwd);
        printf("name     : %s\n", buf_recv.staffData.name);
        printf("phone    : %s\n", buf_recv.staffData.phone);
        printf("post     : %s\n", buf_recv.staffData.post);
        printf("sex      : %s\n", buf_recv.staffData.sex);
        printf("addr     : %s\n", buf_recv.staffData._addr);
        printf("birth    : %s\n", buf_recv.staffData._birth);
        printf("hiredata : %s\n", buf_recv.staffData._hiredata);
        printf("money    : %s\n", buf_recv.staffData._money);

        printf("\n该员工信息如上 按回车继续\n");
    }

    while ('\n' != getchar()) {
        ;
    }
}

/***********************************************
 * 功能：
 *      普通员工：查询自己的历史记录
 * 参数：
 *      sfd：       套接字文件描述符
 *      username:   普通员工名
 * 返回：
 *      void
 ***********************************************/
static void normal_query_history(int sfd, char username[])
{
    int     res             = -1;
    Msg_clientToServer      buf_send;
    Msg_serverToClient      buf_recv;

    buf_send.modeRoot = NORMAL_QUERY_HISTORY;

    strcpy(buf_send.userData._uid, username);
    strcpy(buf_send.userData._pwd, username);   //此时的_pwd充当查询人员用户名

    res = send(sfd, &buf_send, sizeof(buf_send), 0);
    if (0 > res) {
        ERR_CLIENT_CLOSE("send");
    }

    while (1) {
        res = recv(sfd, &buf_recv, sizeof(buf_recv), 0);
        if (0 > res) {
            ERR_CLIENT_CLOSE("recv");
        }
        if (0 == res) {
            SERVER_CLOSE;
        }

        //查到数据的情况
        if (QUERY_OK == buf_recv.answer) {
            printf("%s : %s\n", buf_recv.historyData.time, buf_recv.historyData.history);
        }
        //数据已列完的情况
        if (QUERY_END == buf_recv.answer) {
            printf("\n历史记录已列完 请按回车继续\n");
            break;
        }
        //数据为空的情况
        if (QUERY_NO_FIND == buf_recv.answer) {
            printf("输入的数据有误 请按回车继续n");
            break;
        }
    }

    while ('\n' != getchar()) {
        ;
    }
}

/***********************************************
 * 功能：
 *      普通员工：查询所有员工的信息
 * 参数：
 *      sfd：       套接字文件描述符
 *      username:   普通员工名
 * 返回：
 *      void   
 ***********************************************/
static void normal_query_all(int sfd, char username[])
{
    int         res         = -1;
    Msg_clientToServer      buf_send;
    Msg_serverToClient      buf_recv;

    buf_send.modeRoot = NORMAL_QUERY_STAFF_ALL;
    strcpy(buf_send.userData._pwd, username);   //此时的_pwd充当查询人员用户名

    res = send(sfd, &buf_send, sizeof(buf_send), 0);
    if (0 > res) {
        ERR_CLIENT_CLOSE("send");
    }

    while (1) {
        res = recv(sfd, &buf_recv, sizeof(buf_recv), 0);
        if (0 > res) {
            ERR_CLIENT_CLOSE("recv");
        }
        if (0 == res) {
            SERVER_CLOSE;
        }

        //查到数据的情况
        if (QUERY_OK == buf_recv.answer) {
            printf("\n");
            printf("uid      : %s\n", buf_recv.staffData._user._uid);
            printf("pwd      : %s\n", buf_recv.staffData._user._pwd);
            printf("name     : %s\n", buf_recv.staffData.name);
            printf("phone    : %s\n", buf_recv.staffData.phone);
            printf("post     : %s\n", buf_recv.staffData.post);
            printf("sex      : %s\n", buf_recv.staffData.sex);
            printf("addr     : %s\n", buf_recv.staffData._addr);
            printf("birth    : %s\n", buf_recv.staffData._birth);
            printf("hiredata : %s\n", buf_recv.staffData._hiredata);
            printf("money    : %s\n", buf_recv.staffData._money);
        }
        //数据已列完的情况
        if (QUERY_END == buf_recv.answer) {
            printf("\n所有员工信息已列完 请按回车继续\n");
            break;
        }
        //数据为空的情况
        if (QUERY_NO_FIND == buf_recv.answer) {
            printf("输入的数据有误 请按回车继续n");
            break;
        }
    }

    while ('\n' != getchar()) {
        ;
    }
}

