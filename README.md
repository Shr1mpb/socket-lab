# 计算机网络 - WebServer实现 HTTP/1.1

This repository contains the starter code for ***CMU 15-441/641 Networking and the Internet Project 1: A Web Server Called Liso***.
这里是CMU 卡内基梅隆大学的WebServer实验课程


`实验步骤`大致为：
> 安装Docker并配置好环境
> 逐步实现HTTP/1.1协议的内容
> 包括但不限于对GET/HEAD/POST等请求的处理以及实现管线化
> 进阶：实现高并发处理(使用I/O多路复用)


`实验收获`大致为：
> 深刻记牢HTTP/1.1的各种特性，并了解其底层的实现原理
> 熟练使用socket和c语言的各种字符串、内存操作的方法
> 了解python脚本的用法(可能) 这里写了一些脚本在other文件夹中可供参考或运行


shr1mp 始于 2025.4.3 结于 2025.4.29

# 本人的话


于2025.4.29


本学期开始本人学习了一些redis相关的知识，了解了Redis单线程却性能那么高的原因之一
正是因为使用了epoll IO多路复用机制，于是一开始就带着epoll做了这个实验 以至于week4的内容已融入了前三周


完成这个实验令我本人最自豪的点是，我几乎完全没有使用官方提供的parse.c等文件，而是选择了从头到尾自己实现


但是还有很多不足的地方：
仓库里还有一些瑕疵例如我调试时用的一些输出的语句还没有删除，有时候返回的错误码不正确(这里举例，版本不支持的时候应该先返回505，但如果方法也不对的话我这里会返回501错误)
server.c文件有1400多行，其中可能有五六百甚至七八百行都是重复的代码，由于这边写了各种循环，封装成方法又比较麻烦，就没有处理


完成历程：
从前期通过一些AI的帮助搭建了初步的框架，了解了一些情况的写法与处理缓冲区时的技巧
到中期基本是对半开，自己写一部分，ai写一部分
再到后期week3的实现pipelining，完全自己手写实现
手搓了一个支持高并发的HTTP/1.1服务器的成就感还是有的，虽然瑕疵有点多，但也是本人为数不多坚持下来的项目


建议有时间或者学校需要完成这个实验的同学认真对待，会有一定收获，但是不知道以后能不能用得上 (


# 官方快速开始教程

## 1. Files
- `DockerFile`: Script to build the docker image for the project's environment.
- `Makefile`: Contains rules for `make`.
- `README.md`: Current document.
- `cp1`: CP1 scripts and examples.
- `cp2`: CP2 scripts and examples.
- `cp3`: CP3 scripts and examples.
- `src/`: Source code for the project.
    - `src/echo_client.c`: Simple echo network client.
    - `src/echo_server.c`: Simple echo network server
    - `src/example.c`: Example driver for parsing.
    - `src/lexer.l`: Lex/Yacc related logic.
    - `src/parser.y`
    - `src/parse.c`
- `include/parse.h`

## 2. Environment Setup
1. Install docker: https://www.docker.com
2. Open a terminal and navigate to the directory containing this `README.md` file.
3. Build the docker image: `docker build -t 15-441/641-project-1:latest -f ./DockerFile .`
4. Run the docker container: ``docker run -it -v `pwd`:/home/project-1/ --name <name for your container> 15-441/641-project-1 /bin/bash``
5. The starter code for the project is available at `/home/project-1/` in the container and `.` on your local machine. To make development easier, a mapping is established between these two folders. Modiying the code in one location will also effect the other one. This means that you can use an IDE to write code on your local machine and then seamlessly test it in the container.
6. To test your server using a web browser, you need to configure port mapping for the docker container. Simply add the argument `-p 8888:15441` to the `docker run` command to establish a mapping from `127.0.0.1:15441` in the container to `127.0.0.1:8888` on your local machine. Then you can test your server by using a web browser (e.g., Chrome) on your local machine to navigate to the URL `127.0.0.1:8888`.
