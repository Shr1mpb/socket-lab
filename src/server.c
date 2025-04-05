#include "server.h"

char *bad_request = "HTTP/1.1 400 Bad request\r\n\r\n";
char *not_implemented = "HTTP/1.1 501 Not Implemented\r\n\r\n";

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

void handle_signal(int sig) {
    printf("\nClosing server socket...byebye\n");
    if (global_sock != -1) {
        close(global_sock);
    }
    exit(EXIT_SUCCESS);
}

void init_server(){
	// 注册信号处理器 回调handle_signal关闭socket
	signal(SIGINT, handle_signal); // 处理CTRL+C产生的信号 
    signal(SIGTERM, handle_signal);// 处理KILL产生的信号 
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
	global_sock = sock;  // 将套接字保存到全局变量 处理信号时使用
	// 允许端口复用 避免TCP一直占用端口重启后监听失败
	int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
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
	
	// 把上面完成初始化的所有值都赋给Server结构体
	int port = ECHO_PORT;
	server.epoll_fd = &epoll_fd;
	server.sock = &sock;
	server.ev = &ev;
	server.addr = &addr;
	server.events = events;
	server.fd_to_index = fd_to_index;
	server.port = &port;
	server.current_clients = &current_clients;
	server.clients = clients;


    printf("Server running on port %d (single-threaded epoll), author:shr1mp\n", *server.port);
}

void handle_events(){
	// 取出服务器变量
	int epoll_fd = *server.epoll_fd;
	int sock = *server.sock;
	int *fd_to_index = server.fd_to_index;
	int current_clients = *server.current_clients;
	struct epoll_event ev = *server.ev;
	struct epoll_event *events = server.events;
	Client *clients = server.clients;

	
	// 监听事件发生 并调用对应的处理器
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
                ev.events = EPOLLIN;
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
					client->buf[client->buf_len] = '\0'; // 确保字符串终止

					char *request_end = strstr(client->buf, "\r\n\r\n"); // 检测完整HTTP头
					if (!request_end) {
						// 请求不完整时保持读取
						if (client->buf_len == BUF_SIZE) { // 缓冲区已满仍无完整头
							size_t resp_len = strlen(bad_request);
							// 将错误响应写入缓冲区
							if (resp_len > BUF_SIZE) resp_len = BUF_SIZE;  // 防溢出
							memcpy(client->buf, bad_request, resp_len);
							client->buf_len = resp_len;
							
							// 切换为写事件
							ev.events = EPOLLOUT;
							ev.data.fd = fd;
							if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
								perror("epoll_ctl");
							}
							continue;  // 跳过后续处理
						}
						continue;
					}
			
					// 严格解析请求行
					char *line_end = strstr(client->buf, "\r\n");
					if (!line_end) {// 找不到请求行
						size_t resp_len = strlen(bad_request);
							// 将错误响应写入缓冲区
							if (resp_len > BUF_SIZE) resp_len = BUF_SIZE;  // 防溢出
							memcpy(client->buf, bad_request, resp_len);
							client->buf_len = resp_len;
							
							// 切换为写事件
							ev.events = EPOLLOUT;
							ev.data.fd = fd;
							if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
								perror("epoll_ctl");
							}
							continue;  // 跳过后续处理
					}
					// 截断协议并验证：这里先只验证 路径 和 协议
					// ------------------------开始验证---------------------------------
					*line_end = '\0';// 截断协议用于下面验证


					// 分割请求行各部分
					char *method_end0 = strchr(client->buf, ' ');
					char *path_start0 = method_end0 ? method_end0 + 1 : NULL;
					char *path_end0 = strchr(method_end0 ? method_end0 + 1 : client->buf, ' ');
					char *proto_start0 = path_end0 ? path_end0 + 1 : NULL;

					// 验证请求行基本结构
					if (!method_end0 || !path_end0 || !proto_start0 || // 校验是否有空格分割的三个部分
						(proto_start0 - client->buf) >= BUF_SIZE ||  // 防止越界
						strcmp(proto_start0, "HTTP/1.1") != 0 || // 校验协议
						strncmp(path_start0, "/", 1) != 0 // 校验路径以 / 开头
					) {
							size_t resp_len = strlen(bad_request);
							// 将错误响应写入缓冲区
							if (resp_len > BUF_SIZE) resp_len = BUF_SIZE;  // 防溢出
							memcpy(client->buf, bad_request, resp_len);
							client->buf_len = resp_len;
							
							// 切换为写事件
							ev.events = EPOLLOUT;
							ev.data.fd = fd;
							if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
								perror("epoll_ctl");
							}
							continue;  // 跳过后续处理
					}

					*line_end = '\r';// 把截断的协议字符串复原
					// ------------------------结束验证---------------------------------


					// 方法截断并验证
					if (!method_end0) {
						size_t resp_len = strlen(bad_request);
							// 将错误响应写入缓冲区
							if (resp_len > BUF_SIZE) resp_len = BUF_SIZE;  // 防溢出
							memcpy(client->buf, bad_request, resp_len);
							client->buf_len = resp_len;
							
							// 切换为写事件
							ev.events = EPOLLOUT;
							ev.data.fd = fd;
							if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
								perror("epoll_ctl");
							}
							continue;  // 跳过后续处理
					}
			
					// 提取方法（安全拷贝）
					char method[16] = {0};
					strncpy(method, client->buf, method_end0 - client->buf);
					method[15] = '\0'; // 强制截断防止溢出
			
					// 方法验证
					if (strcmp(method, "GET") != 0 && 
						strcmp(method, "HEAD") != 0 && 
						strcmp(method, "POST") != 0) 
					{
						size_t resp_len = strlen(not_implemented);
							// 将错误响应写入缓冲区
							if (resp_len > BUF_SIZE) resp_len = BUF_SIZE;  // 防溢出
							memcpy(client->buf, not_implemented, resp_len);
							client->buf_len = resp_len;
							
							// 切换为写事件
							ev.events = EPOLLOUT;
							ev.data.fd = fd;
							if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
								perror("epoll_ctl");
							}
							continue;  // 跳过后续处理
					}
			
					// 严格协议版本检查
					char *proto_start = strstr(client->buf, "HTTP/1.1");
					if (!proto_start || (proto_start - client->buf) > 128) { // 协议字段位置异常
						size_t resp_len = strlen(bad_request);
							// 将错误响应写入缓冲区
							if (resp_len > BUF_SIZE) resp_len = BUF_SIZE;  // 防溢出
							memcpy(client->buf, bad_request, resp_len);
							client->buf_len = resp_len;
							
							// 切换为写事件
							ev.events = EPOLLOUT;
							ev.data.fd = fd;
							if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
								perror("epoll_ctl");
							}
							continue;  // 跳过后续处理
					}
			
					/* 构造ECHO响应 */
					size_t req_total_len = request_end - client->buf + 4; // 包含\r\n\r\n
					char header[128];
					int header_len = snprintf(header, sizeof(header),
						"HTTP/1.1 200 OK\r\n"
						"Content-Length: %zd\r\n"
						"Connection: close\r\n\r\n",  // 简化处理，每次关闭连接
						req_total_len);
			
					// 缓冲区安全检查
					if ((size_t)header_len + req_total_len > BUF_SIZE) {
						const char *server_error = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnect: close\r\n\r\n";
						size_t resp_len = strlen(server_error);
							// 将错误响应写入缓冲区
							if (resp_len > BUF_SIZE) resp_len = BUF_SIZE;  // 防溢出
							memcpy(client->buf, server_error, resp_len);
							client->buf_len = resp_len;
							
							// 切换为写事件
							ev.events = EPOLLOUT;
							ev.data.fd = fd;
							if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
								perror("epoll_ctl");
							}
							continue;  // 跳过后续处理
					}
			
					// 移动原始请求数据(请求头)
					memmove(client->buf + header_len, client->buf, req_total_len);
					// 添加响应头
					memcpy(client->buf, header, header_len);
					// 更新缓冲区长度
					client->buf_len = header_len + req_total_len;
			
					// 切换为写模式
					ev.events = EPOLLOUT | EPOLLET;
					ev.data.fd = fd;
					epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
				}
				else if (readret <= 0 && errno != EAGAIN) {
					close_client(epoll_fd, clients, fd_to_index, fd);
					clients[idx].fd = -1;
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
                    if (sent < client->buf_len) {// 没写完 继续监听可写事件
                        memmove(client->buf, client->buf + sent, client->buf_len - sent);
                        client->buf_len -= sent;
                    } else {// 写完了
                        client->buf_len = 0;
                        // 恢复监听读事件
                        ev.events = EPOLLIN;
                        ev.data.fd = fd;
                        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
                    }
                }
                else if (sent == -1 && errno != EAGAIN) {
                    close_client(epoll_fd, clients, fd_to_index, fd);
                    clients[idx].fd = -1;
                    current_clients--;
                }
            }
        }
}
