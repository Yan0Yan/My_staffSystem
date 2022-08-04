#include "server.h"
#include "sql.h"

int main(void)
{
    int         sfd         = 0;        //服务器套接字文件描述符
    int         newfd       = 0;        //与服务器建立连接的客户端文件描述符
    int         cli_addrlen = 0;        //与客户端建立连接的客户端信息结构体长度
    sqlite3     *db         = NULL;     //服务器所维护的数据库
    struct sockaddr_in  sin;            //服务器信息结构体
    struct sockaddr_in  cin;            //与当前客户端建立连接的客户端信息结构体
    ClientData          *p_newcin;      //与客户端建立连接成功后 在新线程中维护的客户端信息结构体
    pthread_t           tid;            //与客户端交互的线程tid
    
    if (sql_init(&db)) {
        fprintf(stderr, "数据库初始化失败 进程退出\n");
		return -1;
	}
    if (server_set(&sfd, &sin, sizeof(sin))){
        fprintf(stderr, "服务器创建失败 进程退出\n");
        return -1;
    }

    printf("服务器Running··· ···\n");
    printf("等待新的客户端连接··· ···\n\n");
    
    while (1) 
    {
        newfd = accept(sfd, (struct sockaddr*)&cin, &cli_addrlen);
        if (newfd < 0) {
            printf("与某客户端建立连接失败\n");
            continue;
        }

        p_newcin = client_setPthreadData(newfd, cin, &db);
        if (NULL == p_newcin) {
            printf("[ %2d : %s : %4d ]: ",
                newfd,  inet_ntoa(cin.sin_addr), ntohs(cin.sin_port));
            printf("堆申请客户端信息失败 主动断开连接\n");
            close(newfd);
            continue;
        }
        
        if (pthread_create(&tid, NULL, client_pthread, (void*)p_newcin)) {
            printf("[ %2d : %s : %4d ]: ",
                newfd,  inet_ntoa(cin.sin_addr), ntohs(cin.sin_port));
            printf("创建客户端交互线程失败 主动断开连接\n");
            free(p_newcin);     //线程创建失败 堆申请的客户端信息结构体由main释放
            close(newfd);
            continue;
        }
    }

    close(sfd);
    return 0;
}