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
int main() {
	init_server();
    while (1) {
        handle_events();
    }
    close(*server.sock);
    return EXIT_SUCCESS;
}
