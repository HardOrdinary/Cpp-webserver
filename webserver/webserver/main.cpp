#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include "locker.h"
#include "threadpool.h"
#include <signal.h>
#include "http_conn.h"

#define MAX_FD 655  //监听
#define MAX_EVENT_NUMBER 10000 //监听的最大的时间数量
/*
当往一个写端关闭的管道或socket连接中连续写入数据时会引发SIGPIPE信号,引发SIGPIPE信号的写操作将设置errno为EPIPE。
在TCP通信中，当通信的双方中的一方close一个连接时，若另一方接着发数据，根据TCP协议的规定，会收到一个RST响应报文，
若再往这个服务器发送数据时，系统会发出一个SIGPIPE信号给进程，告诉进程这个连接已经断开了，不能再写入数据。

*/
// 添加信号
// void(handler)(int)是函数类型，他会自动转换成指向该函数的指针
//void(handler)(int) 等价于


void addsig(int sig, void(handler)(int))
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}
//添加文件描述符到epoll中
extern void addfd(int epollfd, int fd, bool one_shot);
//从epoll中删除文件描述符
extern void removefd(int epollfd,int fd);
//修改文件描述符
extern void modfd(int epollfd,int fd, int ev);
int main(int argc, char *argv[])
{

    // if (argc <= 1)
    // {
    //     printf("按照如下格式运行：%s port_number\n", basename(argv[0]));
    //     exit(-1);
    // }
    // 获取端口号
    int port = 10000;//atoi(argv[1]);

    // 对SIGPIE进行操作
    addsig(SIGPIPE, SIG_IGN);

    //创建线程池，初始化线程池
    threadpool<http_conn>* pool = NULL;
    try {
        pool = new threadpool<http_conn>;
    }catch(...) { //catch(…)能够捕获多种数据类型的异常对象
        exit(-1);
    }
    // 创建一个数组用于保存所有的客户端信息
    http_conn* users = new http_conn[MAX_FD];
    
    //1.创建socket（用于监听）
   int listenfd = socket(AF_INET,SOCK_STREAM, 0);

   if (listenfd == -1) {
    perror("socket");
    exit(-1);
   }
    //设置端口复用:绑定之前设置
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    //绑定
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;//0.0.0.0 ： 表示各网卡绑在一起，就是代表无论哪个IP都可以访问到计算机
    address.sin_port =  htons(port);//主机字节序转网络字节

    int ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    if (ret == -1)
    {
        perror("bind");
        exit(-1);
    }

   //3.监听
    ret = listen(listenfd, 8); // 8是已连接和未连接和的最大值
    if (ret == -1)
    {
        perror("listen");
        exit(-1);
    }
    //创建epoll对象， 事件数组， 添加
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);

    //将监听的文件描述符添加到epoll对象中
    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd;

    while (true) {
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((num < 0) && (errno != EINTR)) {
            printf("epoll failuer");
            break;
        } 
        //循环遍历事件数组
        for(int i = 0; i < num; i++) {

            int sockfd = events[i].data.fd;
            if (sockfd == listenfd) {
                //有客户端连接进来
                struct sockaddr_in client_address;
                socklen_t client_addrlen = sizeof(client_address);

                int connfd= accept(listenfd, (struct sockaddr*)&client_address, &client_addrlen);
                if (http_conn::m_user_count >= MAX_FD) {
                    //目前连接数满了
                    //给客户端写一个信息：服务器内部正忙
                    close(connfd);
                    continue;
                }
                //将新的客户的数据初始化，放在数组中
                users[connfd].init(connfd, client_address);
            } else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                //对方异常断开或错误等事件发生
                users[sockfd].close_conn();

            } else if (events[i].events & EPOLLIN) {
                if(users[sockfd].read()) {
                    //一次性把所有数据读完
                    pool->append(users + sockfd); //数组初始地址 + 数字
                }else {
                    users[sockfd].close_conn();
                }
            }
            else if (events[i].events & EPOLLOUT){
                    //一次性写完所有数据
                    if(!users[sockfd].write()) {
                        users[sockfd].close_conn();
                    }
            }
               
        }
    }
    close(epollfd);
    close(listenfd);
    delete [] users;
    delete pool;

    return 0;
}
