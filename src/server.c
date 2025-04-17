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
// 文件根目录
const char* ROOT_DIR = "/home/project-1/static_site";
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
					char *proto_start = strstr(client->buf, "HTTP/1.1");
					char *proto_start1;
					if (!proto_start || (proto_start - client->buf) > 128) {// 没找到 HTTP/1.1 子字符串 或者 超过128字节(请求行非法)
						if (proto_start1 = strstr(client->buf, "HTTP/")){// 有HTTP/字段 说明协议版本不对
							size_t resp_len = strlen(v_not_supported);
							// 将错误响应写入缓冲区
							if (resp_len > BUF_SIZE) resp_len = BUF_SIZE;  // 防溢出
							memcpy(client->buf, v_not_supported, resp_len);
							client->buf_len = resp_len;

						}else{
							size_t resp_len = strlen(bad_request);
							// 将错误响应写入缓冲区
							if (resp_len > BUF_SIZE) resp_len = BUF_SIZE;  // 防溢出
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
						char last_modified[128];
						get_file_mod_time_rfc1123(full_path, last_modified, sizeof(last_modified));
						// const char *connection_header = client->keep_alive ? "keep-alive" : "close";
						const char *connection_header =  "keep-alive";
						int headers_len = snprintf(headers, sizeof(headers),
							"HTTP/1.1 200 OK\r\n"
							"Content-Type: %s\r\n"
							"Content-Length: %ld\r\n"
							"Last-Modified: %s\r\n"
							"Connection: %s\r\n\r\n",  // 动态设置
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
						}
						
					}else{// 处理Post请求 直接echo回去
						
						
					}
			// // /* 
			// 	//这里week1是echo响应 上面已改为正常处理请求
			// 		// 构造ECHO响应
			// 		size_t req_total_len = request_end - client->buf + 4; // 包含\r\n\r\n
			// 		char header[128];
			// 		int header_len = snprintf(header, sizeof(header),
			// 			"HTTP/1.1 200 OK\r\n"
			// 			"Content-Length: %zd\r\n"
			// 			"Connection: close\r\n\r\n",  // 简化处理，每次关闭连接
			// 			req_total_len);
			
			// 		// 缓冲区安全检查
			// 		if ((size_t)header_len + req_total_len > BUF_SIZE) {
			// 			const char *server_error = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnect: close\r\n\r\n";
			// 			size_t resp_len = strlen(server_error);
			// 				// 将错误响应写入缓冲区
			// 				if (resp_len > BUF_SIZE) resp_len = BUF_SIZE;  // 防溢出
			// 				memcpy(client->buf, server_error, resp_len);
			// 				client->buf_len = resp_len;
							
			// 				// 切换为写事件
			// 				ev.events = EPOLLOUT;
			// 				ev.data.fd = fd;
			// 				if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
			// 					perror("epoll_ctl");
			// 				}
			// 				continue;  // 跳过后续处理
			// 		}
			
			// 		// 移动原始请求数据(请求头)
			// 		memmove(client->buf + header_len, client->buf, req_total_len);
			// 		// 添加响应头
			// 		memcpy(client->buf, header, header_len);
			// 		// 更新缓冲区长度
			// 		client->buf_len = header_len + req_total_len;
			// // */

					free(path); // 释放路径内存
					// 切换为写模式
					ev.events = EPOLLOUT;
					ev.data.fd = fd;
					epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
				}
				else if (readret <= 0 && errno != EAGAIN) { // 没有能读到的了 关闭连接
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

				if(client->file_offset == -1){// 可以一次发送完 直接发送完
					ssize_t sent = send(fd, client->buf, client->buf_len, 0);

					if (sent > 0) {
						if (sent < client->buf_len) {// 本次发送缓冲区没写完 继续监听可写事件
							memmove(client->buf, client->buf + sent, client->buf_len - sent);
							client->buf_len -= sent;
						} else {// 写完了
							// 根据keep-alive决定是否关闭连接
							if (!client->keep_alive) {
								close_client(epoll_fd, clients, fd_to_index, fd);
							} else {
								// 重置缓冲区准备接收新请求
								client->buf_len = 0;
								ev.events = EPOLLIN;
								epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
							}
						}
					}
					else if (sent == -1 && errno != EAGAIN) {
						// 根据keep-alive决定是否关闭连接
						if (!client->keep_alive) {
							close_client(epoll_fd, clients, fd_to_index, fd);
						} else {
							// 重置缓冲区准备接收新请求
							client->buf_len = 0;
							ev.events = EPOLLIN;
							epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
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
									// 重置缓冲区准备接收新请求
									client->buf_len = 0;
									ev.events = EPOLLIN;
									epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
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
									// 继续监听可写事件
									printf("successfully set epollout\n");
									
								} else if (bytes_read == -1 && errno != EAGAIN) {
									printf("bytes_read failed! file_offset - >%zd , file_size -> %ld \n ",client->file_offset, client->file_size);
									// 文件读取错误 直接关闭连接
									close_client(epoll_fd, clients, fd_to_index, fd);
								}

							}

							
						}
					}
					else if (sent == -1 && errno != EAGAIN) {
						printf("file_offset - >%zd , file_size -> %ld 5555555555555\n ",client->file_offset, client->file_size);
						// 根据keep-alive决定是否关闭连接
						if (!client->keep_alive) {
							close_client(epoll_fd, clients, fd_to_index, fd);
						} else {
							// 重置缓冲区准备接收新请求
							client->buf_len = 0;
							ev.events = EPOLLIN;
							epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
						}
					}
				}
            }
			
        }
}
