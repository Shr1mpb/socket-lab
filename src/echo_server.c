/*
*​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​
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
​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**​​**
​*/

/*
    使用epoll I/O多路复用 事件驱动型单线程WebServer
    模仿redis的单线程设计
    fd_to_index是为了防止fd过大导致的Client数组越界
*/
#include "server.h"
extern Server server;
int main(int argc, char *argv[]) {
	char *exec_path = argv[0];
    if (realpath(exec_path, ROOT_DIR) != NULL) {
        // 找到最后一个 '/' 并截断（去掉文件名）
        char *last_slash = strrchr(ROOT_DIR, '/');
        if (last_slash != NULL) {
            *last_slash = '\0';  // 截断到目录部分
        }
        
        // 拼接 "/static_site"
        strncat(ROOT_DIR, "/static_site", sizeof(ROOT_DIR) - strlen(ROOT_DIR) - 1);
        
        printf("Final ROOT_DIR: %s\n", ROOT_DIR);
    } else {
        perror("realpath() failed");
        return 1;
    }

	init_server();
    while (1) {
        handle_events();
    }
    close(*server.sock);
    return EXIT_SUCCESS;
}
