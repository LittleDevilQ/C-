#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>      // 提供IP地址转换函数
#include <netinet/in.h>     // 定义数据结构sockaddr_in
#include <sys/epoll.h>
#include <iostream>
#include <string>
#include "Timer.h"

constexpr uint32_t BUFFER_MAX_LEN = 4096;

void HandleHealth(int clientSock)
{
    char buf[BUFFER_MAX_LEN] { 0 };
    std::string heartMsg = "ping";
    int sendLen = send(clientSock, heartMsg.data(), heartMsg.size(), 0);
    if (sendLen <= 0) {
        std::cout << "send failed\n";
        return;
    }
    std::cout << "send data to server on success, data: [" << heartMsg << "]\n";

    memset(buf, 0, sizeof(buf));
    int recvLen = recv(clientSock, buf, sizeof(buf), 0);
    if (recvLen <= 0) {
        std::cout << "recv failed\n";
        return;
    }
    std::cout << "receive data from server on success, data: [" << buf << "]\n";
}

int main()
{
    // 创建套接字
    int on = 1;
    int clientSock = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSock < 0) {
        std::cout << "create sock failed\n";
        return 0;
    }
    setsockopt(clientSock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    // 向服务器发起请求
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;    // 使用IPv4地址
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");    // ip地址转换
    serverAddr.sin_port = htons(1111);
    auto ret = connect(clientSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (ret < 0) {
        std::cout << "connect failed\n";
        close(clientSock);
        return 0;
    }

    std::cout << "success connect to epoll server\n";

    Timer clientTimer;
    clientTimer.Start(5, HandleHealth, clientSock);
    getchar();

    close(clientSock);
    return 0;
}