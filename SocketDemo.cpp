#include <string>
#include <iostream>
#include <algorithm>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>      // 提供IP地址转换函数
#include <netinet/in.h>     // 定义数据结构sockaddr_in
#include <sys/epoll.h>
#include <fcntl.h>
#include "ThreadPool.h"

constexpr uint32_t EPOLL_MAX_NUM = 2048;
constexpr uint32_t BUFFER_MAX_LEN = 4096;

//设置socket连接为非阻塞模式
void setnonblocking (int fd)
{
	int opts;
 
	opts = fcntl (fd, F_GETFL);
	if (opts < 0)
	{
		perror ("fcntl(F_GETFL)\n");
		exit (1);
	}
	opts = (opts | O_NONBLOCK);
	if (fcntl (fd, F_SETFL, opts) < 0)
	{
		perror ("fcntl(F_SETFL)\n");
		exit (1);
	}
}

void Read(int epfd, epoll_event& activeEvent)
{
    int n = 0;
    int nread = 0;
    char buf[BUFFER_MAX_LEN] { 0 };
    while ((nread = read(activeEvent.data.fd, buf + n, BUFFER_MAX_LEN)) > 0) {
        n += nread;
    }
    if (nread == -1 && errno != EAGAIN) {
        std::cout << "read data err\n";
        return;
    }
    std::cout << "read data from client [" << buf << "]\n";
    
    memset(buf, 0, sizeof(buf));
    snprintf (buf, sizeof(buf), "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nHello World", 11);
    int nwrite = 0;
    int dataSize = strlen(buf);
    n = dataSize;
    while (n > 0) {
        nwrite = write(activeEvent.data.fd, buf + dataSize - n, n);
        if (nwrite < n) {
            if (nwrite == -1 && errno != EAGAIN) {
                perror ("write error");
            }
            break;
        }
        n -= nwrite;
    }
    std::cout << "send data to client [" << buf << "]\n";
}

void Write(int epfd, epoll_event& activeEvent)
{
    char buf[BUFFER_MAX_LEN] { 0 };
    snprintf (buf, sizeof(buf), "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nHello World", 11);
    int nwrite = 0;
    int dataSize = strlen(buf);
    int n = dataSize;
    while (n > 0) {
        nwrite = write(activeEvent.data.fd, buf + dataSize - n, n);
        if (nwrite < n) {
            if (nwrite == -1 && errno != EAGAIN) {
                perror ("write error");
            }
            break;
        }
        n -= nwrite;
    }
    std::cout << "send data to client [" << buf << "]\n";
    struct epoll_event event;
    event.data.fd = activeEvent.data.fd;
    event.events = activeEvent.events | EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_MOD, activeEvent.data.fd, &event);
    // close(activeEvent.data.fd);
    // activeEvent.data.fd = -1;
}

int main()
{
    // 创建并启动线程池
    ThreadPool pool(20);
    pool.Start();

    // 1.创建套接字
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        std::cout << "create sock failed\n";
        return 0;
    }
    int opt = SO_REUSEADDR;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setnonblocking(serverSock);

    // 2.将套接字和IP、端口绑定
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;    // 使用IPv4地址
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");    // ip地址转换
    serverAddr.sin_port = htons(1111);
    auto ret = bind(serverSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (ret < 0) {
        std::cout << "bind failed\n";
        close(serverSock);
        return 0;
    }

    // 3.进入监听状态，等待用户发起请求
    listen(serverSock, 20);

    // 4.创建epoll
    int epfd = epoll_create(EPOLL_MAX_NUM);
    if (epfd < 0) {
        std::cout << "create epoll failed\n";
        close(serverSock);
        return 0;
    }

    // 5.socket -> epoll
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = serverSock;
    // 通过epoll_ctl将server socket作为事件注册到内核
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, serverSock, &event) < 0) {
        std::cout << "create epoll failed\n";
        close(serverSock);
        close(epfd);
        return 0;
    }

    // 循环等待客户端连接事件
    int clientFd = 0;
    int connFd = 0;
    struct sockaddr_in clientAddr {};
    socklen_t clientLen;
    char buf[BUFFER_MAX_LEN] { 0 };
    struct epoll_event* activeEvents = (struct epoll_event*)malloc(sizeof(struct epoll_event) * EPOLL_MAX_NUM);
    while (true) {
        // activeEvents 表示内核监听到并通知过来的事件集合，EPOLL_MAX_NUM 表示可接受的最大事件数，
        // activeFdCount 表示实际返回的事件数，-1表示永久阻塞等待
        int activeFdCount = epoll_wait(epfd, activeEvents, EPOLL_MAX_NUM, -1);
        for (int i = 0; i < activeFdCount; ++i) {
            clientFd = activeEvents[i].data.fd;
            if (activeEvents[i].data.fd == serverSock) {
                while ((connFd = accept(serverSock, (struct sockaddr*)&clientAddr, &clientLen)) > 0) {
                    char* clientIp = inet_ntoa(clientAddr.sin_addr);
                    std::cout << "accept client connection from [" << clientIp << ":" << clientAddr.sin_port << "]\n";
                    setnonblocking(connFd);   // 设置连接非阻塞
                    event.events = EPOLLIN | EPOLLET;   // 边沿触发要求套接字为非阻塞模式；水平触发可以是阻塞或非阻塞模式
                    event.data.fd = connFd;
                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, connFd, &event) < 0) {
                        std::cout << "epoll add failed\n";
                        continue;
                    }
                }
                if (connFd == -1) {
                    if (errno != EAGAIN && errno != ECONNABORTED && errno != EPROTO && errno != EINTR) {
                        std::cout << "epoll add failed\n";
                    }
				}
				continue;
            }
            if (activeEvents[i].events & EPOLLIN) {
                auto f = pool.Submit(Read, epfd, activeEvents[i]);
            }
        }
    }
    // 关闭套接字
    close(serverSock);
    close(epfd);

    return 0;
}