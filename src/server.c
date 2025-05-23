#include "server.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

char *bad_request = "HTTP/1.1 400 Bad request\r\n\r\n";
char *not_implemented = "HTTP/1.1 501 Not Implemented\r\n\r\n";

// 第二周要添加的返回类型 400 404 501 505
const char *not_found = "HTTP/1.1 404 Not Found\r\n\r\n";
const char *v_not_supported = "HTTP/1.1 505 HTTP Version not supported\r\n\r\n";

// 500 Internal server error 处理内部错误
const char *internal_error = "HTTP/1.1 500 Internal server error\r\n\r\n";

// http的最长url
const int PATH_MAX = 2083;

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
// 获取文件扩展名并确定MIME类型
const char* get_mime_type(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot) return "application/octet-stream";
    
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0) return "text/html";
    if (strcmp(dot, ".txt") == 0) return "text/plain";
    if (strcmp(dot, ".css") == 0) return "text/css";
    if (strcmp(dot, ".js") == 0) return "application/javascript";
    if (strcmp(dot, ".json") == 0) return "application/json";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(dot, ".png") == 0) return "image/png";
    if (strcmp(dot, ".gif") == 0) return "image/gif";
    if (strcmp(dot, ".svg") == 0) return "image/svg+xml";
    if (strcmp(dot, ".pdf") == 0) return "application/pdf";
    
    return "application/octet-stream";
}

// 获取当前时间的RFC1123格式字符串
void get_current_time_rfc1123(char *buf, size_t buf_size) {
    time_t now = time(NULL);
    struct tm tm;
    gmtime_r(&now, &tm);
    strftime(buf, buf_size, "%a, %d %b %Y %H:%M:%S GMT", &tm);
}

// 获取文件最后修改时间的RFC1123格式字符串
void get_file_mod_time_rfc1123(const char *filename, char *buf, size_t buf_size) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        struct tm tm;
        gmtime_r(&st.st_mtime, &tm);
        strftime(buf, buf_size, "%a, %d %b %Y %H:%M:%S GMT", &tm);
    } else {
        strncpy(buf, "Thu, 01 Jan 1970 00:00:00 GMT", buf_size);
    }
}

// 获取请求头中的字符串信息
char* get_header_value(const char *headers, const char *key) {
    char search_key[128];
    snprintf(search_key, sizeof(search_key), "\r\n%s: ", key);
    
    char *start = strstr(headers, search_key);
    if (!start) return NULL;
    
    start += strlen(search_key);
    char *end = strstr(start, "\r\n");
    if (!end) return NULL;
    
    size_t len = end - start;
    char *value = malloc(len + 1);
    memcpy(value, start, len);
    value[len] = '\0';
    return value;
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

	int idx = fd_to_index[fd];
	Client *client = &clients[idx];
	// 关闭fd
	if (client->file_fd != -1) {
		close(client->file_fd);
		client->file_fd = -1;
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
	// // 重定向输出到日志文件
	// FILE* log_file = freopen("output.log", "a", stdout);
    // if (!log_file) {
    //     perror("Failed to open log file");
    //     return 1;
    // }


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
            
            // 新客户端连接 当服务端socket被epoll_wait返回时(即可读时) 注意 有连接处于keep-alive状态会使得epoll每次都返回服务端socket的fd
            if (fd == sock) {
                struct sockaddr_in cli_addr;
                socklen_t cli_len = sizeof(cli_addr);
                int client_sock = accept(sock, (struct sockaddr*)&cli_addr, &cli_len);
                
                if (client_sock == -1) {
					if (errno == EAGAIN || errno == EWOULDBLOCK){// 如果是keep-alive，accept会返回这两个状态 直接继续循环就可以
						continue;
					}
					// 是新连接 如果连接错误输出错误日志
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
				client->temp_request_buf_on = 0;
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
				Client *client = &clients[idx];
				ssize_t readret = recv(fd, client->buf + client->buf_len, BUF_SIZE - client->buf_len, 0);
				int request_count = 0;
				if (readret > 0) {
					client->buf_len += readret;
					client->buf[client->buf_len] = '\0';
			
					// 混合处理逻辑核心
					char *current_ptr = client->buf;
					
					// 循环检测完整请求头
					while ((request_count < MAX_PIPELINE_REQUESTS) && 
						  (current_ptr = strstr(current_ptr, "\r\n\r\n"))) {
						request_count++;
						current_ptr += 4; // 移动到下一个请求起始位置
					}
				}else if (readret <= 0 && errno != EAGAIN) { // 没有能读到的了 关闭连接
					close_client(epoll_fd, clients, fd_to_index, fd);
					clients[idx].fd = -1;
					current_clients--;
				}
				if (request_count > 1)// pipeline请求 新逻辑
				{
					printf("Pipeline %d requests...\n",request_count);
					char *current_ptr = client->buf;
					// 建立缓冲区 存储每个请求
					// char *pipeline_requests = malloc(client->buf_len + request_count);
					char *pipeline_requests = malloc(client->buf_len + request_count);
					char *pipeline_requests_ptr = pipeline_requests;
					int temp = request_count;
					// 一个一个把请求放在缓冲区中 这里使用pipeline_requests_ptr来迭代处理
					while (temp-- > 0){
						if(temp == request_count - 1 && client->temp_request_buf_on == 1){// 第一次的pipeline请求 并且上次的请求没有处理完(开启了临时请求缓冲区)
							printf("\n\n\n\n\n\n\nUSE TEMPBUF\n\n\n\n\n\n\n\n");
							// 将上次未处理完的内容填充到client->buf的前端
							memmove(client->buf + client->temp_request_buf_size,client->buf, BUF_SIZE - client->temp_request_buf_size);
							memcpy(client->buf,client->temp_request_buf,client->temp_request_buf_size);
						}
						char *request_end = strstr(current_ptr, "\r\n\r\n"); // 获取当前请求
						if (!request_end) break;

						// 计算单个请求长度
						size_t request_len = request_end - current_ptr + 4;

						// 生成独立请求缓冲区
						char *single_request = malloc(request_len + 1);
						memcpy(single_request, current_ptr, request_len);
						single_request[request_len] = '\0';

						// 将独立请求缓冲区中的内容放在pipeline_requests中
						memcpy(pipeline_requests_ptr, single_request, request_len + 1);

						// 移动处理指针
						current_ptr += request_len;
						pipeline_requests_ptr += request_len;
						// 存储这个请求的大小
						client->last_request_len = request_len;
						// 释放内存
						free(single_request);
					}
					// 处理完毕后 看看后面是否还有多余的没解析完的请求
					if(strstr(current_ptr,"\r\n")){
						printf("\n\n\n\n\n\n\nTEMP BUF ON...\n\n\n\n\n\n\n");
						// 启用临时缓冲区
						client->temp_request_buf_on = 1;
						// 记录多余内容的大小
						int exceeded_size = BUF_SIZE - (current_ptr - client->buf);
						client->temp_request_buf_size = exceeded_size;
						// 把多余的请求内容放在临时缓冲区
						memcpy(client->temp_request_buf, current_ptr, exceeded_size);

					}

					// printf("pipeline_requests: \n %s \n",pipeline_requests);
					// 一个一个处理请求 这里使用pipeline_requests_ptrr来迭代处理
					char* pipeline_requests_ptrr = pipeline_requests;
					client->file_offset = -1; // 设置为一次可以全发送完
					char* client_response_buf = malloc(BUF_SIZE);
					int client_response_buf_len = 0;
					char* client_response_buf_ptr = client_response_buf;
					
					int total_count = request_count;
					while(request_count-- > 0){
						char* request_start = pipeline_requests_ptrr;
						char* request_end = strstr(pipeline_requests_ptrr,"\r\n\r\n");
						if (!request_end)
						{
							break;
						}
						
						int request_len = request_end - request_start + 4;
						*request_end = '\0';
						printf("request %d is:\n%s\n",total_count - request_count,request_start);
						*request_end = '\r';
						// 开始处理请求
							// 严格解析请求行
							char *line_end = strstr(request_start, "\r\n");
							if (!line_end) {// 找不到请求行
								size_t resp_len = strlen(bad_request);
									// 将错误响应写入缓冲区
									// 比缓冲区大小大了 直接截断(忽略了当前请求) 然后切换到写事件 发送
									if (client_response_buf_len + resp_len > BUF_SIZE) {
										client_response_buf_len = BUF_SIZE;
										client_response_buf[BUF_SIZE] = '\0';
										// 切换为写事件
										ev.events = EPOLLOUT;
										ev.data.fd = fd;
										if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
											perror("epoll_ctl");
										}
										continue;// 跳过后续while(1)中的处理

									}
									pipeline_requests_ptrr += request_len;
									// 还没到缓冲区大小限制
									memcpy(client_response_buf_ptr, bad_request, resp_len);
									client_response_buf_len += resp_len;
									client_response_buf_ptr += resp_len;
									// printf("client_response_buf1: \n%s\n",client_response_buf);
									continue;;// 跳过后续while(1)中的处理
									
							}
							// 截断协议并验证：这里先只验证 路径 和 协议
							// ------------------------开始验证---------------------------------
							*line_end = '\0';// 截断协议用于下面验证

							// 分割请求行各部分
							char *method_end0 = strchr(request_start, ' ');
							char *path_start0 = method_end0 ? method_end0 + 1 : NULL;
							char *path_end0 = strchr(method_end0 ? method_end0 + 1 : request_start, ' ');
							char *proto_start0 = path_end0 ? path_end0 + 1 : NULL;

							// 验证请求行基本结构
							if (!method_end0 || !path_end0 || !proto_start0 || // 校验是否有空格分割的三个部分
								(proto_start0 - client->buf) >= BUF_SIZE ||  // 防止越界
								strncmp(path_start0, "/", 1) != 0 // 校验路径以 / 开头
							) {
								size_t resp_len = strlen(bad_request);
								// 将错误响应写入缓冲区
								// 比缓冲区大小大了 直接截断(忽略了当前请求) 然后切换到写事件 发送
								if (client_response_buf_len + resp_len > BUF_SIZE) {
									client_response_buf_len = BUF_SIZE;
									client_response_buf[BUF_SIZE] = '\0';
									// 切换为写事件
									ev.events = EPOLLOUT;
									ev.data.fd = fd;
									if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
										perror("epoll_ctl");
									}
									// printf("client_response_buf2: \n%s\n",client_response_buf);
									continue;// 跳过后续while(1)中的处理

								}
								pipeline_requests_ptrr += request_len;
								// 还没到缓冲区大小限制
								memcpy(client_response_buf_ptr, bad_request, resp_len);
								client_response_buf_len += resp_len;
								client_response_buf_ptr += resp_len;
								continue;// 跳过后续while(1)中的处理
							}

							*line_end = '\r';// 把截断的协议字符串复原
							// ------------------------结束验证---------------------------------
							// 提取path:
							// 提取路径
							size_t path_len = path_end0 - path_start0;
							char *path = malloc(path_len + 1);  // +1 为了结尾的\0
							memcpy(path, path_start0, path_len);
							path[path_len] = '\0';  // 结束
							
							// 验证路径合法性
							if (strstr(path, "..")) {
								size_t resp_len = strlen(bad_request);
								// 将错误响应写入缓冲区
								// 比缓冲区大小大了 直接截断(忽略了当前请求) 然后切换到写事件 发送
								if (client_response_buf_len + resp_len > BUF_SIZE) {
									client_response_buf_len = BUF_SIZE;
									client_response_buf[BUF_SIZE] = '\0';
									// 切换为写事件
									ev.events = EPOLLOUT;
									ev.data.fd = fd;
									if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
										perror("epoll_ctl");
									}
									// printf("client_response_buf3: \n%s\n",client_response_buf);
									continue;// 跳过后续while(1)中的处理

								}
								pipeline_requests_ptrr += request_len;
								// 还没到缓冲区大小限制
								memcpy(client_response_buf_ptr, bad_request, resp_len);
								client_response_buf_len += resp_len;
								client_response_buf_ptr += resp_len;
								// printf("client_response_buf4: \n%s\n",client_response_buf);
								continue;// 跳过后续while(1)中的处理
							}
							// 方法截断并验证
							if (!method_end0) {
								size_t resp_len = strlen(bad_request);
								// 将错误响应写入缓冲区
								// 比缓冲区大小大了 直接截断(忽略了当前请求) 然后切换到写事件 发送
								if (client_response_buf_len + resp_len > BUF_SIZE) {
									client_response_buf_len = BUF_SIZE;
									client_response_buf[BUF_SIZE] = '\0';
									// 切换为写事件
									ev.events = EPOLLOUT;
									ev.data.fd = fd;
									if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
										perror("epoll_ctl");
									}
									// printf("client_response_buf5: \n%s\n",client_response_buf);
									continue;// 跳过后续while(1)中的处理

								}
								pipeline_requests_ptrr += request_len;
								// 还没到缓冲区大小限制
								memcpy(client_response_buf_ptr, bad_request, resp_len);
								client_response_buf_len += resp_len;
								client_response_buf_ptr += resp_len;
								// printf("client_response_buf6: \n%s\n",client_response_buf);
								continue;// 跳过后续while(1)中的处理
							}
							// 提取方法（安全拷贝）
							char method[16] = {0};
							strncpy(method, request_start, method_end0 - request_start);
							method[15] = '\0'; // 强制截断防止溢出
							// 方法验证
							if (strcmp(method, "GET") != 0 && 
								strcmp(method, "HEAD") != 0 && 
								strcmp(method, "POST") != 0) 
							{
								size_t resp_len = strlen(not_implemented);
								// 将错误响应写入缓冲区
								// 比缓冲区大小大了 直接截断(忽略了当前请求) 然后切换到写事件 发送
								if (client_response_buf_len + resp_len > BUF_SIZE) {
									client_response_buf_len = BUF_SIZE;
									client_response_buf[BUF_SIZE] = '\0';
									// 切换为写事件
									ev.events = EPOLLOUT;
									ev.data.fd = fd;
									if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
										perror("epoll_ctl");
									}
									// printf("client_response_buf7: \n%s\n",client_response_buf);
									continue;// 跳过后续while(1)中的处理

								}
								pipeline_requests_ptrr += request_len;
								// 还没到缓冲区大小限制
								memcpy(client_response_buf_ptr, not_implemented, resp_len);
								client_response_buf_len += resp_len;
								client_response_buf_ptr += resp_len;
								// printf("client_response_buf8: \n%s\n",client_response_buf);
								continue;// 跳过后续while(1)中的处理
							}
						

							// 严格协议版本检查
							char *method_end = strchr(request_start, ' ');
							char *path_start = method_end ? method_end + 1 : NULL;
							char *path_end = path_start ? strchr(path_start, ' ') : NULL;
							char *proto_start = path_end ? path_end + 1 : NULL;
							if (!proto_start || !line_end || (line_end - proto_start) < 8 || 
							strncmp(proto_start, "HTTP/1.1", 8) != 0 || 
							((line_end - proto_start) > 8  && (proto_start[8] != '\r'))) {
									// 协议版本不是 HTTP/1.1
									if (strstr(request_start, "HTTP/")) {
										size_t resp_len = strlen(v_not_supported);
										// 将错误响应写入缓冲区
										// 比缓冲区大小大了 直接截断(忽略了当前请求) 然后切换到写事件 发送
										if (client_response_buf_len + resp_len > BUF_SIZE) {
											printf("error6\n");
											client_response_buf_len = BUF_SIZE;
											client_response_buf[BUF_SIZE] = '\0';
											// 切换为写事件
											ev.events = EPOLLOUT;
											ev.data.fd = fd;
											if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
												perror("epoll_ctl");
											}
											// printf("client_response_buf9: \n%s\n",client_response_buf);
											continue;// 跳过后续while(1)中的处理

										}
										pipeline_requests_ptrr += request_len;
										// 还没到缓冲区大小限制
										memcpy(client_response_buf_ptr, v_not_supported, resp_len);
										client_response_buf_len += resp_len;
										client_response_buf_ptr += resp_len;
										// printf("client_response_buf10: \n%s\n",client_response_buf);
										continue;// 跳过后续while(1)中的处理
									}else {
										size_t resp_len = strlen(not_implemented);
										// 将错误响应写入缓冲区
										// 比缓冲区大小大了 直接截断(忽略了当前请求) 然后切换到写事件 发送
										if (client_response_buf_len + resp_len > BUF_SIZE) {
											printf("error7\n");
											client_response_buf_len = BUF_SIZE;
											client_response_buf[BUF_SIZE] = '\0';
											// 切换为写事件
											ev.events = EPOLLOUT;
											ev.data.fd = fd;
											if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
												perror("epoll_ctl");
											}
											// printf("client_response_buf11: \n%s\n",client_response_buf);
											continue;// 跳过后续while(1)中的处理

										}
										pipeline_requests_ptrr += request_len;
										// 还没到缓冲区大小限制
										memcpy(client_response_buf_ptr, not_implemented, resp_len);
										client_response_buf_len += resp_len;
										client_response_buf_ptr += resp_len;
										// printf("client_response_buf12: \n%s\n",client_response_buf);
										continue;// 跳过后续while(1)中的处理
									}
							}
							// 读取请求头
							char q_headers[BUF_SIZE];
							int q_headers_len;
							if (request_end) {
								// 计算请求头长度（包含最后的 \r\n\r\n）
								size_t headers_total_len = request_end - request_start + 4;
								
								// 将头信息复制到专用缓冲区
								memcpy(q_headers, request_start, headers_total_len);
								q_headers_len = headers_total_len;
								q_headers[q_headers_len] = '\0';
								
								// // 剩余数据是请求体（如果有）这里假定pipeline请求没有请求体
								// size_t body_len = request_end - headers_total_len;
								// memmove(request_start, request_start + headers_total_len, body_len);
								// client_response_buf_len= body_len;
							}

							char query_headers[4096]; // 获取请求头中的keep_alive部分
							memcpy(query_headers, q_headers, q_headers_len + 1); // +1 复制终止符
							size_t copy_len = MIN(q_headers_len, sizeof(query_headers)-1);
							query_headers[copy_len] = '\0';
							char *connection = get_header_value(query_headers, "Connection");
							client->keep_alive = (connection && strcasecmp(connection, "keep-alive") == 0);
							if(!connection){// 如果没有 默认关闭连接
								client->keep_alive = 0;
							}
							// 构造响应printf("New client: %s:%d (fd=%d)\n", client->ipstr, client->port, client_sock);
							printf("Generate the response to client %s:%d%s(fd=%d)\n", client->ipstr, client->port, path, client->fd);
							// 处理GET/HEAD 
							if (strcmp(method, "GET") == 0 || strcmp(method, "HEAD") == 0) {
								// 获取文件元数据
								struct stat st;
								char full_path[PATH_MAX];

								if (strcmp(path, "/") == 0) {  
									// 访问根目录，返回 index.html  
									snprintf(full_path, sizeof(full_path), "%s/index.html", ROOT_DIR);  
								} else {   
									// 防止目录穿越攻击在之前读出path的时候已经做过
									snprintf(full_path, sizeof(full_path), "%s%s", ROOT_DIR, path);  
								}  

								// 使用 stat 判断文件是否存在且是普通文件  
								if (stat(full_path, &st) == -1 || !S_ISREG(st.st_mode)) {  
									printf("file not found\n"); // 记录日志
									size_t resp_len = strlen(not_found);
										// 将错误响应写入缓冲区
										// 比缓冲区大小大了 直接截断(忽略了当前请求) 然后切换到写事件 发送
										if (client_response_buf_len + resp_len > BUF_SIZE) {;
											client_response_buf_len = BUF_SIZE;
											client_response_buf[BUF_SIZE] = '\0';
											// 切换为写事件
											ev.events = EPOLLOUT;
											ev.data.fd = fd;
											if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
												perror("epoll_ctl");
											}
											// printf("client_response_buf13: \n%s\n",client_response_buf);
											continue;// 跳过后续while(1)中的处理

										}
										pipeline_requests_ptrr += request_len;
										// 还没到缓冲区大小限制
										memcpy(client_response_buf_ptr, not_found, resp_len);
										client_response_buf_len += resp_len;
										client_response_buf_ptr += resp_len;
										// printf("client_response_buf14: \n%s\n",client_response_buf);
										continue;// 跳过后续while(1)中的处理
									
								}  

								// 打开文件
								int file_fd = open(full_path, O_RDONLY);
								if (file_fd == -1) {// 打开失败 返回错误
									printf("file not found\n"); // 记录日志
									size_t resp_len = strlen(internal_error);
										// 将错误响应写入缓冲区
										// 比缓冲区大小大了 直接截断(忽略了当前请求) 然后切换到写事件 发送
										if (client_response_buf_len + resp_len > BUF_SIZE) {
											client_response_buf_len = BUF_SIZE;
											client_response_buf[BUF_SIZE] = '\0';
											// 切换为写事件
											ev.events = EPOLLOUT;
											ev.data.fd = fd;
											if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
												perror("epoll_ctl");
											}
											// printf("client_response_buf15: \n%s\n",client_response_buf);
											continue;// 跳过后续while(1)中的处理

										}
										pipeline_requests_ptrr += request_len;
										// 还没到缓冲区大小限制
										memcpy(client_response_buf_ptr, internal_error, resp_len);
										client_response_buf_len += resp_len;
										client_response_buf_ptr += resp_len;
										// printf("client_response_buf16: \n%s\n",client_response_buf);
										continue;// 跳过后续while(1)中的处理
								}

								
								// 使用文件描述符获取状态
								if (fstat(file_fd, &st) == -1) { // 获取失败
									printf("file not found\n"); // 记录日志
									size_t resp_len = strlen(internal_error);
										// 将错误响应写入缓冲区
										// 比缓冲区大小大了 直接截断(忽略了当前请求) 然后切换到写事件 发送
										if (client_response_buf_len + resp_len > BUF_SIZE) {
											printf("error10\n");
											client_response_buf_len = BUF_SIZE;
											client_response_buf[BUF_SIZE] = '\0';
											// 切换为写事件
											ev.events = EPOLLOUT;
											ev.data.fd = fd;
											if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
												perror("epoll_ctl");
											}
											// printf("client_response_buf17: \n%s\n",client_response_buf);
											continue;// 跳过后续while(1)中的处理

										}
										pipeline_requests_ptrr += request_len;
										// 还没到缓冲区大小限制
										memcpy(client_response_buf_ptr, internal_error, resp_len);
										client_response_buf_len += resp_len;
										client_response_buf_ptr += resp_len;
										// printf("client_response_buf18: \n%s\n",client_response_buf);
										continue;// 跳过后续while(1)中的处理
								}

								// 动态构造响应头
								char headers[512];
								// 获取文件信息
								const char *mime_type = get_mime_type(full_path);
								// 获取时间信息
								char date_buf[64];
								get_current_time_rfc1123(date_buf, sizeof(date_buf));
								char last_modified[128];
								get_file_mod_time_rfc1123(full_path, last_modified, sizeof(last_modified));
								const char *connection_header = client->keep_alive ? "keep-alive" : "close";
								int headers_len = snprintf(headers, sizeof(headers),
									"HTTP/1.1 200 OK\r\n"
									"Server: liso/1.1\r\n"
									"Date: %s\r\n"
									"Content-Type: %s\r\n"
									"Content-Length: %ld\r\n"
									"Last-Modified: %s\r\n"
									"Connection: %s\r\n\r\n",  // 动态设置
									date_buf,
									mime_type,
									st.st_size,
									last_modified,
									connection_header);
								// 处理响应头缓冲区溢出
								if (headers_len >= (int)sizeof(headers)) {
									printf("file not found\n"); // 记录日志
									size_t resp_len = strlen(internal_error);
										// 将错误响应写入缓冲区
										// 比缓冲区大小大了 直接截断(忽略了当前请求) 然后切换到写事件 发送
										if (client_response_buf_len + resp_len > BUF_SIZE) {
											printf("error11\n");
											client_response_buf_len = BUF_SIZE;
											client_response_buf[BUF_SIZE] = '\0';
											// 切换为写事件
											ev.events = EPOLLOUT;
											ev.data.fd = fd;
											if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
												perror("epoll_ctl");
											}
											// printf("client_response_buf19: \n%s\n",client_response_buf);
											continue;// 跳过后续while(1)中的处理

										}
										pipeline_requests_ptrr += request_len;
										// 还没到缓冲区大小限制
										memcpy(client_response_buf_ptr, internal_error, resp_len);
										client_response_buf_len += resp_len;
										client_response_buf_ptr += resp_len;
										// printf("client_response_buf20: \n%s\n",client_response_buf);
										continue;// 跳过后续while(1)中的处理
								}
								// 填充响应缓冲区
								memcpy(client_response_buf_ptr, headers, headers_len);
								client_response_buf_len += headers_len;
								client_response_buf_ptr += headers_len;
							

								// GET方法需要发送文件内容（HEAD不发送）
								// 这里管线化比较难实现 直接不返回文件
								// if (strcmp(method, "GET") == 0) {
								// 	// 先设置文件状态
								// 	client->file_fd = file_fd;
								// 	client->file_offset = 0;
								// 	client->file_size = st.st_size;
								// 	client->header_out = 0;
									
								// 	// 开始读取
								// 	ssize_t bytes_read = pread(file_fd, client_response_buf_ptr + headers_len, MIN(BUF_SIZE - headers_len, client->file_size), client->file_offset);
								// 	if (bytes_read > 0) { // 读取成功 
								// 		if(client->file_size <= BUF_SIZE - headers_len){// 文件大小比缓冲区大小要小
								// 			client->buf_len += bytes_read; // 直接加上 然后设置offset为-1 表示已经发送完 在写事件时直接发送即可
								// 			client->file_offset = -1;
								// 		}else{// 文件大小比缓冲区大小要大 设置偏移量为读取的大小，并在可写事件时继续发送
								// 			// 不做处理 等可写时发送
								// 		}
								// 	} else if (bytes_read == -1) {// 读取文件失败
								// 		size_t resp_len = strlen(internal_error);
								// 		// 将错误响应写入缓冲区
								// 		// 比缓冲区大小大了 直接截断(忽略了当前请求) 然后切换到写事件 发送
								// 		if (client_response_buf_len + resp_len > BUF_SIZE) {
								// 			printf("error12\n");
								// 			client_response_buf_len = BUF_SIZE;
								// 			client_response_buf[BUF_SIZE] = '\0';
								// 			// 切换为写事件
								// 			ev.events = EPOLLOUT;
								// 			ev.data.fd = fd;
								// 			if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
								// 				perror("epoll_ctl");
								// 			}
								// 			// printf("client_response_buf21: \n%s\n",client_response_buf);
								// 			continue;// 跳过后续while(1)中的处理

								// 		}
								// 		pipeline_requests_ptrr += request_len;
								// 		// 还没到缓冲区大小限制
								// 		memcpy(client_response_buf_ptr, internal_error, resp_len);
								// 		client_response_buf_len += resp_len;
								// 		client_response_buf_ptr += resp_len;
								// 		// printf("client_response_buf22: \n%s\n",client_response_buf);
								// 		continue;// 跳过后续while(1)中的处理
								// 	}
								// }else{// HEAD 不返回文件
								// 	client->file_offset = -1;
								// }
								client->file_offset = -1;
							}else if(strcmp(method, "POST") == 0){// 处理Post请求 直接echo回去
								// TODO: 
								size_t req_total_len = request_end - request_start + 4; // 包含\r\n\r\n
								char header[128];
								int header_len = snprintf(header, sizeof(header),
									"HTTP/1.1 200 OK\r\n"
									"Content-Length: %zd\r\n"
									"Connection: close\r\n\r\n",  // 简化处理，每次关闭连接
									req_total_len);
						
								// 移动原始请求数据(放在响应头后)
								memmove(client->buf + header_len, client->buf, req_total_len);
								// 添加响应头
								memcpy(client->buf, header, header_len);
								// 更新缓冲区长度
								client->buf_len = header_len + req_total_len;
								
								
							}else{// 出错
								size_t resp_len = strlen(internal_error);
								// 将错误响应写入缓冲区
								// 比缓冲区大小大了 直接截断(忽略了当前请求) 然后切换到写事件 发送
								if (client_response_buf_len + resp_len > BUF_SIZE) {
									printf("error13\n");
									client_response_buf_len = BUF_SIZE;
									client_response_buf[BUF_SIZE] = '\0';
									// 切换为写事件
									ev.events = EPOLLOUT;
									ev.data.fd = fd;
									if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
										perror("epoll_ctl");
									}
									// printf("client_response_buf23: \n%s\n",client_response_buf);
									continue;// 跳过后续while(1)中的处理

								}
								// 还没到缓冲区大小限制
								pipeline_requests_ptrr += request_len;
								memcpy(client_response_buf_ptr, internal_error, resp_len);
								client_response_buf_len += resp_len;
								client_response_buf_ptr += resp_len;
								// printf("client_response_buf24: \n%s\n",client_response_buf);
								continue;// 跳过后续while(1)中的处理
							}
							// 成功后迭代读取请求时的指针
							pipeline_requests_ptrr += request_len;
							free(path); // 释放路径内存
							// printf("client_response_buf25: \n%s\n",client_response_buf);
							printf("\n\n\n\nhere\n\n\n\n");
							printf("reqeust_count = %d\n",request_count);
							continue;
						
					}
					printf("total client_response_buf: \n%s\n",client_response_buf);
					free(pipeline_requests);// 释放内存
					// 将要写的内容拷贝到缓冲区
					memcpy(client->buf, client_response_buf, client_response_buf_len);
					client->buf_len = client_response_buf_len;
					// 切换为写事件
					printf("change to writeable...\n");
					ev.events = EPOLLOUT;
					ev.data.fd = fd;
					if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
						perror("epoll_ctl");
					}
					continue;  // 跳过后续处理
					
				}
				else{ // 只有单个请求 走原本的逻辑处理单个请求即可
					printf("Single request...\n");
					printf("\n\n%s\n\n",client->buf);
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
					*request_end = '\0';
					printf("Received request:\n%s\n",client->buf);
					*request_end = '\r';
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
					// 提取path:
					// 提取路径
					size_t path_len = path_end0 - path_start0;
					char *path = malloc(path_len + 1);  // +1 为了结尾的\0
					memcpy(path, path_start0, path_len);
					path[path_len] = '\0';  // 结束
					
					// 验证路径合法性
					if (strstr(path, "..")) {
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
							continue; // 跳过后续处理
					}
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
					char *method_end = strchr(client->buf, ' ');
					char *path_start = method_end ? method_end + 1 : NULL;
					char *path_end = path_start ? strchr(path_start, ' ') : NULL;
					char *proto_start = path_end ? path_end + 1 : NULL;
					if (!proto_start || !line_end || (line_end - proto_start) < 8 || 
					strncmp(proto_start, "HTTP/1.1", 8) != 0 || 
					((line_end - proto_start) > 8  && (proto_start[8] != '\r'))) {
						 	// 协议版本不是 HTTP/1.1
							 if (strstr(client->buf, "HTTP/")) {
								size_t resp_len = strlen(v_not_supported);
								memcpy(client->buf, v_not_supported, resp_len);
								client->buf_len = resp_len;
							}else {
								size_t resp_len = strlen(bad_request);
								memcpy(client->buf, bad_request, resp_len);
								client->buf_len = resp_len;
							}
							// 切换为写事件
							ev.events = EPOLLOUT;
							ev.data.fd = fd;
							if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
								perror("epoll_ctl");
							}
							continue;  // 跳过后续处理
					}

					// 读取请求头
					char q_headers[BUF_SIZE];
					int q_headers_len;
					if (request_end) {
						// 计算请求头长度（包含最后的 \r\n\r\n）
						size_t headers_total_len = request_end - client->buf + 4;
						
						// 将头信息复制到专用缓冲区
						memcpy(q_headers, client->buf, headers_total_len);
						q_headers_len = headers_total_len;
						q_headers[q_headers_len] = '\0';
						
						// 剩余数据是请求体（如果有）
						size_t body_len = client->buf_len - headers_total_len;
						memmove(client->buf, client->buf + headers_total_len, body_len);
						client->buf_len = body_len;
					}

					char query_headers[4096]; // 获取请求头中的keep_alive部分
					memcpy(query_headers, q_headers, q_headers_len + 1); // +1 复制终止符
					size_t copy_len = MIN(q_headers_len, sizeof(query_headers)-1);
					query_headers[copy_len] = '\0';
					char *connection = get_header_value(query_headers, "Connection");
					client->keep_alive = (connection && strcasecmp(connection, "keep-alive") == 0);
					if(!connection){// 如果没有 默认关闭连接
						client->keep_alive = 0;
					}
					// 构造响应printf("New client: %s:%d (fd=%d)\n", client->ipstr, client->port, client_sock);
					printf("Generate the response to client %s:%d%s(fd=%d)\n", client->ipstr, client->port, path, client->fd);
					// 处理GET/HEAD 
					if (strcmp(method, "GET") == 0 || strcmp(method, "HEAD") == 0) {
						// 获取文件元数据
						struct stat st;
						char full_path[PATH_MAX];

						if (strcmp(path, "/") == 0) {  
							// 访问根目录，返回 index.html  
							snprintf(full_path, sizeof(full_path), "%s/index.html", ROOT_DIR);  
						} else {   
							// 防止目录穿越攻击在之前读出path的时候已经做过
							snprintf(full_path, sizeof(full_path), "%s%s", ROOT_DIR, path);  
						}  

						// 使用 stat 判断文件是否存在且是普通文件  
						if (stat(full_path, &st) == -1 || !S_ISREG(st.st_mode)) {  
							// 文件不存在，返回错误响应  
							size_t resp_len = strlen(not_found);
							// 将错误响应写入缓冲区
							if (resp_len > BUF_SIZE) resp_len = BUF_SIZE;  // 防溢出
							memcpy(client->buf, not_found, resp_len);
							client->buf_len = resp_len;
							printf("file not found\n"); // 记录日志
							// 切换为写事件
							ev.events = EPOLLOUT;
							ev.data.fd = fd;
							if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
								perror("epoll_ctl");
							}
							continue;  // 跳过后续处理
						}  

						// 打开文件
						int file_fd = open(full_path, O_RDONLY);
						if (file_fd == -1) {// 打开失败 返回错误
							size_t resp_len = strlen(internal_error);
							// 将错误响应写入缓冲区
							if (resp_len > BUF_SIZE) resp_len = BUF_SIZE;  // 防溢出
							memcpy(client->buf, internal_error, resp_len);
							client->buf_len = resp_len;
							// 切换为写事件
							ev.events = EPOLLOUT;
							ev.data.fd = fd;
							if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
								perror("epoll_ctl");
							}
							continue;  // 跳过后续处理
						}

						
						// 使用文件描述符获取状态
						if (fstat(file_fd, &st) == -1) { // 获取失败
							close(file_fd);
							size_t resp_len = strlen(internal_error);
							// 将错误响应写入缓冲区
							if (resp_len > BUF_SIZE) resp_len = BUF_SIZE;  // 防溢出
							memcpy(client->buf, internal_error, resp_len);
							client->buf_len = resp_len;
							// 切换为写事件
							ev.events = EPOLLOUT;
							ev.data.fd = fd;
							if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
								perror("epoll_ctl");
							}
							continue;  // 跳过后续处理
						}

						// 动态构造响应头
						char headers[512];
						// 获取文件信息
						const char *mime_type = get_mime_type(full_path);
						// 获取时间信息
						char date_buf[64];
						get_current_time_rfc1123(date_buf, sizeof(date_buf));
						char last_modified[128];
						get_file_mod_time_rfc1123(full_path, last_modified, sizeof(last_modified));
						const char *connection_header = client->keep_alive ? "keep-alive" : "close";
						int headers_len = snprintf(headers, sizeof(headers),
							"HTTP/1.1 200 OK\r\n"
							"Server: liso/1.1\r\n"
							"Date: %s\r\n"
							"Content-Type: %s\r\n"
							"Content-Length: %ld\r\n"
							"Last-Modified: %s\r\n"
							"Connection: %s\r\n\r\n",  // 动态设置
							date_buf,
							mime_type,
							st.st_size,
							last_modified,
							connection_header);
						// 处理响应头缓冲区溢出
						if (headers_len >= (int)sizeof(headers)) {
							close(file_fd);
							size_t resp_len = strlen(internal_error);
							// 将错误响应写入缓冲区
							if (resp_len > BUF_SIZE) resp_len = BUF_SIZE;  // 防溢出
							memcpy(client->buf, internal_error, resp_len);
							client->buf_len = resp_len;
							// 切换为写事件
							ev.events = EPOLLOUT;
							ev.data.fd = fd;
							if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
								perror("epoll_ctl");
							}
							continue;  // 跳过后续处理
						}
						// 填充响应缓冲区
						memcpy(client->buf, headers, headers_len);
						client->buf_len = headers_len;

						// GET方法需要发送文件内容（HEAD不发送）
						if (strcmp(method, "GET") == 0) {
							// 先设置文件状态
							client->file_fd = file_fd;
							client->file_offset = 0;
							client->file_size = st.st_size;
							client->header_out = 0;
							
							// 开始读取
							ssize_t bytes_read = pread(file_fd, client->buf + headers_len, MIN(BUF_SIZE - headers_len, client->file_size), client->file_offset);
							if (bytes_read > 0) { // 读取成功 
								if(client->file_size <= BUF_SIZE - headers_len){// 文件大小比缓冲区大小要小
									client->buf_len += bytes_read; // 直接加上 然后设置offset为-1 表示已经发送完 在写事件时直接发送即可
									client->file_offset = -1;
								}else{// 文件大小比缓冲区大小要大 设置偏移量为读取的大小，并在可写事件时继续发送
									// 不做处理 等可写时发送
								}
							} else if (bytes_read == -1) {// 读取文件失败
								size_t resp_len = strlen(internal_error);
								// 将错误响应写入缓冲区
								if (resp_len > BUF_SIZE) resp_len = BUF_SIZE;  // 防溢出
								memcpy(client->buf, internal_error, resp_len);
								client->buf_len = resp_len;
								// 切换为写事件
								ev.events = EPOLLOUT;
								ev.data.fd = fd;
								if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
									perror("epoll_ctl");
								}
								continue;  // 跳过后续处理
							}
						}else{
							client->file_offset = -1;
						}
						
					}else if(strcmp(method, "POST") == 0){// 处理Post请求 直接echo回去
						size_t req_total_len = request_end - client->buf + 4; // 包含\r\n\r\n
						char header[128];
						int header_len = snprintf(header, sizeof(header),
							"HTTP/1.1 200 OK\r\n"
							"Content-Length: %zd\r\n"
							"Connection: close\r\n\r\n",  // 简化处理，每次关闭连接
							req_total_len);
				
						// 缓冲区安全检查
						if ((size_t)header_len + req_total_len > BUF_SIZE) {
							const char *server_error = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
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
				
						// 移动原始请求数据(放在响应头后)
						memmove(client->buf + header_len, client->buf, req_total_len);
						// 添加响应头
						memcpy(client->buf, header, header_len);
						// 更新缓冲区长度
						client->buf_len = header_len + req_total_len;
						
						
					}else{// 出错
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

					free(path); // 释放路径内存
					// 切换为写模式
					ev.events = EPOLLOUT;
					ev.data.fd = fd;
					epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
				}
			}
            // 客户端可写事件
            else if (events[i].events & EPOLLOUT) {
				printf("writeable...\n");
                int idx = fd_to_index[fd];
                if (idx == -1 || idx >= MAX_CLIENTS) continue;
                
                Client *client = &clients[idx];

				if(client->file_offset == -1){// 可以一次发送完 直接发送完
					printf("once send\n");
					ssize_t sent = send(fd, client->buf, client->buf_len, 0);

					if (sent > 0) {
						printf("sent > 0 and sent = %d\n",sent);
						if (sent < client->buf_len) {// 本次发送缓冲区没写完 继续监听可写事件
							printf("not send completely...\n");
							memmove(client->buf, client->buf + sent, client->buf_len - sent);
							client->buf_len -= sent;
						} else {// 写完了
							printf("send completely successful...\n");
							// 根据keep-alive决定是否关闭连接
							if (!client->keep_alive) {
								close_client(epoll_fd, clients, fd_to_index, fd);
							} else {
								// 完全重置客户端状态
								client->buf_len = 0;
								client->file_offset = -1;
								client->file_size = 0;
								client->header_out = 0;
								if (client->file_fd != -1) {
									close(client->file_fd);
									client->file_fd = -1;
								}
								
								// 重新注册EPOLLIN事件
								ev.events = EPOLLIN;
								ev.data.fd = fd;
								if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
									perror("epoll_ctl mod failed");
									close_client(epoll_fd, clients, fd_to_index, fd);
								}
							}
						}
					}
					else if (sent == -1 && errno != EAGAIN) {
						// 根据keep-alive决定是否关闭连接
						if (!client->keep_alive) {
							close_client(epoll_fd, clients, fd_to_index, fd);
						} else {
							// 完全重置客户端状态
							client->buf_len = 0;
							client->file_offset = -1;
							client->file_size = 0;
							client->header_out = 0;
							if (client->file_fd != -1) {
								close(client->file_fd);
								client->file_fd = -1;
							}
							
							// 重新注册EPOLLIN事件
							ev.events = EPOLLIN;
							ev.data.fd = fd;
							if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
								perror("epoll_ctl mod failed");
								close_client(epoll_fd, clients, fd_to_index, fd);
							}
						}
					}
				}
                else{ // 无法直接发送完 分批次发送 发完一次后重置状态然后继续监听可写事件
					ssize_t sent = send(fd, client->buf, client->buf_len, 0); // 这里如果是第一次发送 就是先发送了响应头
					if (sent > 0) {// 发送成功
						if (sent < client->buf_len) {// 本次发送缓冲区没写完 继续监听可写事件
							memmove(client->buf, client->buf + sent, client->buf_len - sent);
							client->buf_len -= sent;
							client->file_offset += sent;
						} else {// 写完了 但是文件可能尚未发送完成
							if(client->header_out){
								client->file_offset += sent;// 更新已发送的文件字节数
							}else{
								client->header_out = 1;
							}
							
							// 如果文件已发送完成
							if(client->file_offset >= client->file_size){
								printf("Complete ! file_offset - >%zd , file_size -> %ld\n",client->file_offset, client->file_size);
								
								// 根据keep-alive决定是否关闭连接
								if (!client->keep_alive) {
									close_client(epoll_fd, clients, fd_to_index, fd);
								} else {
									// 完全重置客户端状态
									client->buf_len = 0;
									client->file_offset = -1;
									client->file_size = 0;
									client->header_out = 0;
									if (client->file_fd != -1) {
										close(client->file_fd);
										client->file_fd = -1;
									}
									
									// 重新注册EPOLLIN事件
									ev.events = EPOLLIN;
									ev.data.fd = fd;
									if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
										perror("epoll_ctl mod failed");
										close_client(epoll_fd, clients, fd_to_index, fd);
									}
								}
							}else{// 文件还没发送完
								// 缓冲区大小置0
								client->buf_len = 0;
								
								 // 计算剩余需要读取的字节数
								off_t remaining = client->file_size - client->file_offset;
								size_t to_read = MIN(BUF_SIZE, remaining);
								
								// 从文件读取下一块数据
								ssize_t bytes_read = pread(client->file_fd, client->buf, to_read, client->file_offset);
								printf("bytes_read = %ld \n", bytes_read);
								if (bytes_read > 0) {
									client->buf_len = bytes_read;
									
								} else if (bytes_read == -1 && errno != EAGAIN) {
									printf("bytes_read failed! file_offset - >%zd , file_size -> %ld \n ",client->file_offset, client->file_size);
									// 文件读取错误 直接关闭连接
									close_client(epoll_fd, clients, fd_to_index, fd);
								}

							}

							
						}
					}
					else if (sent == -1 && errno != EAGAIN) {
						// 根据keep-alive决定是否关闭连接
						if (!client->keep_alive) {
							close_client(epoll_fd, clients, fd_to_index, fd);
						} else {
							// 完全重置客户端状态
							client->buf_len = 0;
							client->file_offset = -1;
							client->file_size = 0;
							client->header_out = 0;
							if (client->file_fd != -1) {
								close(client->file_fd);
								client->file_fd = -1;
							}
							
							// 重新注册EPOLLIN事件
							ev.events = EPOLLIN;
							ev.data.fd = fd;
							if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
								perror("epoll_ctl mod failed");
								close_client(epoll_fd, clients, fd_to_index, fd);
							}
						}
					}
				}
            }
			
        }
}
