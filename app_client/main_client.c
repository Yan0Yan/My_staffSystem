#include "client.h"

int main(void)
{
    int     sfd         = 0;
    struct  sockaddr_in sin;

    printf("客户端初始化......\n");
    if (connect_server(&sfd, &sin, sizeof(sin))) {
        printf("与服务器建立连接失败 退出进程\n");
        return -1;
    }

    printf("客户端将在1秒后进入界面 请稍后...\n");
    sleep(1);

    //进入欢迎界面 正式与服务器进行交互
    do {
        welcome_init();
    } while (user_init(sfd));

    close(sfd);
    return 0;
}