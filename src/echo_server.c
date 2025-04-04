/******************************************************************************
* echo_server.c                                                               *
*                                                                             *
* Description: This file contains the C source code for an echo server.  The  *
*              server runs on a hard-coded port and simply write back anything*
*              sent to it by connected clients.  It does not support          *
*              concurrent clients.                                            *
*                                                                             *
* Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                         *
*          Wolf Richter <wolf@cs.cmu.edu>                                     *
*                                                                             *
*******************************************************************************/

/*
	使用epoll I/O多路复用 事件驱动型单线程WebServer
	模仿redis的单线程设计
	fd_to_index是为了防止fd过大导致的Client数组越界
*/
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <errno.h>

#define BUF_SIZE 4096
#define ECHO_PORT 9999
#define MAX_CLIENTS 1024  // 最大客户端数量
#define MAX_EVENTS 1024

// 客户端连接状态
typedef struct {
    int fd;              // 套接字
    char buf[BUF_SIZE];  // 读/写缓冲区
    size_t buf_len;      // 缓冲区当前数据长度
    char ipstr[INET_ADDRSTRLEN]; // 客户端IP地址
    int port;            // 客户端端口
} Client;

// 设置fd为非阻塞模式
int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void close_client(int epoll_fd, Client *clients, int *fd_to_index, int fd) {
    if (fd_to_index[fd] != -1) {
        printf("Client %s:%d disconnected\n", 
               clients[fd_to_index[fd]].ipstr, 
               clients[fd_to_index[fd]].port);
        fd_to_index[fd] = -1;
    }
    if (fd != -1) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
        close(fd);
    }
}

int main() {
    int sock, epoll_fd;
    struct sockaddr_in addr;
    struct epoll_event ev, events[MAX_EVENTS];
    
    // 使用固定大小的客户端数组
    Client clients[MAX_CLIENTS];
    int fd_to_index[MAX_CLIENTS * 2]; // 扩大映射表范围
    memset(fd_to_index, -1, sizeof(fd_to_index));
	// 初始化Clients槽位可用
	for(int i = 0; i < MAX_CLIENTS; i++){
		clients[i].fd = -1;
	}
    
    int current_clients = 0;

    // 初始化TCP套接字
    sock = socket(AF_INET, SOCK_STREAM, 0);
    set_nonblocking(sock);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(ECHO_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    listen(sock, MAX_CLIENTS);

    // 初始化epoll
    epoll_fd = epoll_create1(0);
    ev.events = EPOLLIN;
    ev.data.fd = sock;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &ev); // 将服务端client放入event_poll中

    printf("Server running on port %d (single-threaded epoll), author:shr1mp\n", ECHO_PORT);

    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;
            
            // 新客户端连接
            if (fd == sock) {
                struct sockaddr_in cli_addr;
                socklen_t cli_len = sizeof(cli_addr);
                int client_sock = accept(sock, (struct sockaddr*)&cli_addr, &cli_len);
                
                if (client_sock == -1) {
                    perror("accept failed");
                    continue;
                }

                // 检查是否超过最大客户端数
                if (current_clients >= MAX_CLIENTS) {
                    fprintf(stderr, "Too many clients, rejecting,client fd: %d\n", client_sock);
                    close(client_sock);
                    continue;
                }

                // 找到空闲的客户端槽位
                int client_index = -1;
                for (int j = 0; j < MAX_CLIENTS; j++) {
                    if (clients[j].fd == -1) {
                        client_index = j;
                        break;
                    }
                }

                if (client_index == -1) {
                    fprintf(stderr, "No available client slot\n");
                    close(client_sock);
                    continue;
                }

                // 初始化客户端信息
                set_nonblocking(client_sock);
                Client *client = &clients[client_index];
                client->fd = client_sock;
                client->buf_len = 0;
                inet_ntop(AF_INET, &cli_addr.sin_addr, client->ipstr, INET_ADDRSTRLEN);// 获取IP地址并设置
                client->port = ntohs(cli_addr.sin_port);// 获取端口并设置
                fd_to_index[client_sock] = client_index; // fd会重复利用 这里应该是不会越界 fd_to_index的大小已经是两倍
                current_clients++;

                printf("New client: %s:%d (fd=%d)\n", client->ipstr, client->port, client_sock);

                // 监听可读事件
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client_sock;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_sock, &ev);
            }
            // 客户端可读事件
            else if (events[i].events & EPOLLIN) {
                int idx = fd_to_index[fd];
                if (idx == -1 || idx >= MAX_CLIENTS) continue;
                
                Client *client = &clients[idx];
                ssize_t readret = recv(fd, client->buf + client->buf_len, BUF_SIZE - client->buf_len, 0);

                if (readret > 0) {
                    client->buf_len += readret;
                    ev.events = EPOLLOUT | EPOLLET;
                    ev.data.fd = fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
                }
                else if (readret <= 0 && errno != EAGAIN) {
                    close_client(epoll_fd, clients, fd_to_index, fd);
                    clients[idx].fd = -1; // 标记为可用
                    current_clients--;
                }
            }
            // 客户端可写事件
            else if (events[i].events & EPOLLOUT) {
                int idx = fd_to_index[fd];
                if (idx == -1 || idx >= MAX_CLIENTS) continue;
                
                Client *client = &clients[idx];
                ssize_t sent = send(fd, client->buf, client->buf_len, 0);

                if (sent > 0) {
                    if (sent < client->buf_len) {
                        memmove(client->buf, client->buf + sent, client->buf_len - sent);
                        client->buf_len -= sent;
                    } else {
                        client->buf_len = 0;
                        ev.events = EPOLLIN | EPOLLET;
                        ev.data.fd = fd;
                        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
                    }
                }
                else if (sent == -1 && errno != EAGAIN) {
                    close_client(epoll_fd, clients, fd_to_index, fd);
                    clients[idx].fd = -1; // 标记为可用
                    current_clients--;
                }
            }
        }
    }

    close(sock);
    return EXIT_SUCCESS;
}
