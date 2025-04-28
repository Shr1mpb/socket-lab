#ifndef SERVER_H // 防止重复包含  
#define SERVER_H  
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
#include <signal.h>
#include <sys/stat.h>  // 定义 struct stat
#include <unistd.h>    // 提供 fstat() 等系统调用
#include <time.h>
#include <libgen.h>  // dirname()

#define BUF_SIZE 4096 // 缓冲区大小
#define ECHO_PORT 9999 // 服务器监听的端口
#define MAX_CLIENTS 1024  // 最大客户端数量
#define MAX_EVENTS 1024 // event_poll最大事件数量
#define MAX_PIPELINE_REQUESTS 10 // 最大的pipeline请求个数

char ROOT_DIR[4096];
static volatile int global_sock = -1;

// 客户端连接状态
typedef struct{
    int fd;              // 套接字
    char buf[BUF_SIZE];  // 读/写缓冲区
    size_t buf_len;      // 缓冲区当前数据长度
    char ipstr[INET_ADDRSTRLEN]; // 客户端IP地址
    int port;            // 客户端端口
	int current_clients;
	int keep_alive; 	 // 持久连接
	
    struct iovec *response_queue;  // 响应队列
	
	// 新增文件传输相关字段
    int file_fd;            // 当前传输的文件描述符
    off_t file_offset;      // 当前文件偏移量
    off_t file_size;        // 文件总大小
	int header_out; 		// 响应头是否发送
} Client;

// 存储服务端的一些必要信息
typedef struct{
	int *sock; // 服务端socket
	int *epoll_fd; // epoll多路复用池
	struct epoll_event *ev, *events; // 服务端epoll属性 ev是单个 events是数组
	struct sockaddr_in *addr; // 服务端IP地址
    int *port; // 服务端端口
	Client *clients; // 连接的客户端的数组
	int *fd_to_index;// fd和客户端数组映射关系 数组
	int *current_clients; // 当前客户端个数
} Server;
Server server;// 创建Server结构体对象

// ----------------------函数声明-----------------------
// 初始化服务器
void init_server();
// 监听并处理事件
void handle_events();
// 设置fd为非阻塞模式
int set_nonblocking(int fd);
// 关闭客户端连接并输出日志
void close_client(int epoll_fd, Client *clients, int *fd_to_index, int fd);
// 处理信号 在关闭时输出日志
void handle_signal(int sig);

#endif  