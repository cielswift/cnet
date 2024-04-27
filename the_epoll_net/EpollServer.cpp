//
// Created by cielswifts on 2024/4/6.
//

#include "EpollServer.h"
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <sys/epoll.h>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <pthread.h>
#include <cerrno>

#define null NULL

static char RUN_SERVER = 0;
static unsigned long long REQUEST_COUNT = 0;
static const int MAX_EVENTS = 256;
static const int MAX_CLIENT_SIZE = 1024;
static int debug = 1;

struct SlaveEpollWrap {
    int epollFd;
    int socketFds[1024];
};

struct MainEpollWrap {
    int epollFd;
    int serverSocketFd;
};


void closeClient(int status, int fd, SlaveEpollWrap *pSlaveEpollWrap) {

    char buffer[1]; // 创建一个小缓冲区
    ssize_t n = recv(fd, buffer, 1, MSG_DONTWAIT);
    if (n == 0 || (n == -1 && errno != EAGAIN && errno != EWOULDBLOCK)) {
        if (debug) {
            printf("client %d ,not receive data maybe close>> \n", fd);
        }
    }

    if (debug) {
        printf("close fd  %d ,status %d ,in epoll fd %d ,close data: %s \n", fd, status, pSlaveEpollWrap->epollFd,
               buffer);
    }

    if (epoll_ctl(pSlaveEpollWrap->epollFd, EPOLL_CTL_DEL, fd, null) == -1) {
        perror("del client fd del failed");
    }

    shutdown(fd, SHUT_RDWR);
    //关闭网络套接字
    if (close(fd) == -1) {
        perror("close client fd failed");
    }

    for (int i = 0; i < MAX_CLIENT_SIZE; ++i) {
        int client_fd = pSlaveEpollWrap->socketFds[i];
        if (client_fd == fd) {
            pSlaveEpollWrap->socketFds[i] = 0;
            if (debug) {
                printf("close fd and clear finish --- %d \n", fd);
            }
            break;
        }
    }
}

void readDate(int fd, SlaveEpollWrap *pSlaveEpollWrap) {
    char buffer_receive[1024];
    // 可读事件：已连接套接字有数据可读，可以调用 recv() 或类似函数
    ssize_t n = recv(fd, buffer_receive, 1024, MSG_DONTWAIT);
    if (n > 0) {

        if (debug) {
            printf("client %d ,receive data: %s \n", fd, buffer_receive);
        }


        time_t now;
        time(&now);
        char text[32];
        int realCount = 0;
        memset(text, '\0', sizeof(text));
        sprintf(text, "{\"time\":%ld}", now);
        for (int i = 0; i < 32; ++i) {
            if (text[i] == '\0') {
                break;
            }
            realCount++;
        }
        char rep[64];
        memset(rep, '\0', sizeof(rep));
        sprintf(rep, "HTTP/1.1 200\nContent-Length: %d\n\n%s", realCount, text);

        int realCount2 = 0;
        for (int i = 0; i < 64; ++i) {
            if (rep[i] == '\0') {
                break;
            }
            realCount2++;
        }

        send(fd, rep, realCount2, MSG_DONTWAIT);

    } else {
        if (debug) {
            printf("client %d ,not receive data maybe close \n", fd);
        }
        closeClient(1, fd, pSlaveEpollWrap);
    }
}

void *subReactorProcessOne(void *params) {
    if (params == null) {
        if (debug) {
            printf("error params is null ! \n");
        }
        return params;
    }

    SlaveEpollWrap *pSlaveEpollWrap = (SlaveEpollWrap *) params;
    printf("sub reactor start process epoll fd %d, address %p \n", pSlaveEpollWrap->epollFd, params);
    struct epoll_event events[MAX_EVENTS] = {0};
    for (;;) {
        int num_events = epoll_wait(pSlaveEpollWrap->epollFd, events, MAX_EVENTS, 1000);
        if (num_events <= 0) {
            continue;
        }
        for (int i = 0; i < num_events; ++i) {
            int conn_fd = events[i].data.fd; // 获取触发事件的文件描述符
            uint32_t conn_event_flags = events[i].events; // 获取事件标志
            /**
             * 一旦EPOLLHUP被触发，应当做好不能再读取到任何新数据的准备
             * 并且任何后续的recv()调用都有可能返回0（表示连接已经正常关闭）或-1（表示连接异常关闭
             */
            //正常关闭
            if ((conn_event_flags & EPOLLIN) && (conn_event_flags & EPOLLHUP)) {
                closeClient(0, conn_fd, pSlaveEpollWrap);
                continue;
            }

            //异常关闭
            if ((conn_event_flags & EPOLLHUP)) {
                closeClient(-1, conn_fd, pSlaveEpollWrap);
                continue;
            }


            if ((conn_event_flags & EPOLLERR)) {
                closeClient(-2, conn_fd, pSlaveEpollWrap);
                continue;
            }


            //正常读取
            if ((conn_event_flags & EPOLLIN) != 0) {
                readDate(conn_fd, pSlaveEpollWrap);
                continue;
            }
        }

        if (RUN_SERVER == 0) {
            break;
        }

    }


    for (int i = 0; i < MAX_CLIENT_SIZE; ++i) {
        int client_fd = pSlaveEpollWrap->socketFds[i];
        if (client_fd != 0) {
            //关闭epoll
            if (epoll_ctl(pSlaveEpollWrap->epollFd, EPOLL_CTL_DEL, client_fd, null) == -1) {
                perror("epoll_ctl del failed");
            }
            shutdown(client_fd, SHUT_RDWR);
            //关闭网络套接字
            close(client_fd);
        }
    }
    if (debug) {
        printf("sub reactor end process epoll fd %d \n", pSlaveEpollWrap->epollFd);
    }
    return null;
}



////////////////////////////////////////////////////////////////////////////////////////

void addNewConnectionToEpoll(int conn_sock_fd, SlaveEpollWrap *subReactors) {
    if (debug) {
        printf("new connection fd is %d ,choose sub reactor fd %d\n", conn_sock_fd, subReactors->epollFd);
    }
    /**
     * 返回文件描述符的 状态
    * 这些标志通常是一些宏定义的位掩码，如 O_RDONLY、O_WRONLY、O_RDWR、O_NONBLOCK 等，
     * 可以使用按位与（&）操作符检查特定标志是否已设置
    */
    int flags = fcntl(conn_sock_fd, F_GETFL, 0);
    /**
     * 设置文件描述符 为非阻塞，因为边缘触发模式通常配合非阻塞 I/O 使用
     */
    fcntl(conn_sock_fd, F_SETFL, flags | O_NONBLOCK);

    struct epoll_event sub_conn_event{0};
    /**
     * 监听可读事件 和 挂起事件 并启用边缘触发
     *  EPOLLHUP 连接关闭 当连接断开时，epoll_wait()函数会返回EPOLLHUP事件。同时，由于TCP连接关闭时，TCP栈会在接收缓冲区填充一个 EOF 标记，
     *  EPOLLERR 连接异常
     *
     *  这时 EPOLLIN 事件也会触发
     */
    sub_conn_event.events = EPOLLIN | EPOLLET;
    sub_conn_event.data.fd = conn_sock_fd; // 关联已连接套接字的文件描述符

    if (epoll_ctl(subReactors->epollFd, EPOLL_CTL_ADD, conn_sock_fd, &sub_conn_event) == -1) {
        // 处理错误，如打印错误信息、关闭文件描述符等
        perror("sub_epoll_ctl err");
    }

    for (int i = 0; i < MAX_CLIENT_SIZE; ++i) {
        if (subReactors->socketFds[i] == 0) {
            subReactors->socketFds[i] = conn_sock_fd;
            if (debug) {
                printf("connection fs is registered %d, finish \n", conn_sock_fd);
            }
            break;
        }
    }
}

int createSocketServer(int port) {
    int server_socket_fd; // 服务器端套接字描述符
    /**
     * 由于 server_addr 通常用于存放服务器的网络地址信息（如 IP 地址、端口号等），清零操作确保了结构体内部的所有字段都处于已知的初始状态
     */
    struct sockaddr_in server_addr{0}; // 服务器地址结构
    /**
     * AF_INET：用于 Internet 协议版本 4 (IPv4)，这是最常见的选择，用于基于 TCP/IP 的网络通信
     * SOCK_STREAM :tcp , SOCK_DGRAM:udp
     * 在大多数情况下，可以将其设置为 0
     *
     */
    server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd < 0) {
        perror("Failed to create server socket");
        exit(EXIT_FAILURE);
    }

    /**
     * 这一行代码的作用就是将整个 server_addr 结构体的所有字节清零
     */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /**
     * 不同类型的套接字可能需要不同的地址结构体，例如IPv4套接字使用sockaddr_in结构体，IPv6套接字使用sockaddr_in6结构体。
     * 这些结构体的大小并不相同，因此，bind()函数需要知道实际使用的结构体的确切大小
     */
    if (bind(server_socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to bind server socket");
        exit(EXIT_FAILURE);
    }

    /**
     * 最多允许1024个未完成连接排队
     */
    if (listen(server_socket_fd, 1024) < 0) {
        perror("Failed to listen on server socket");
        exit(EXIT_FAILURE);
    }

    return server_socket_fd;
}


int createEpoll(int server_socket_fd) {
    int epoll_fd = epoll_create1(0); // epoll_create1在较新的Linux内核中更为推荐
    if (epoll_fd <= 0) {
        perror("epoll_create1 failed");
        exit(EXIT_FAILURE);
    }

    struct epoll_event event{0};
    event.events = EPOLLIN | EPOLLET; // 监听可读事件，并使用边缘触发模式（水平触发请去掉EPOLLET）
    event.data.fd = server_socket_fd; // 要监听的套接字文件描述符

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket_fd, &event) == -1) {
        perror("epoll_ctl add failed");
        exit(EXIT_FAILURE);
    }
    return epoll_fd;
}


void startLinuxEpoll(int port, int deBug) {
    debug = deBug;

    int server_socket_fd = createSocketServer(port);
    int epoll_fd = createEpoll(server_socket_fd);
    MainEpollWrap mainEpollWrap = {epoll_fd, server_socket_fd};
    printf("new server fd is %d, main epoll fd is %d \n", server_socket_fd, epoll_fd);

    //从reactor 根据可用的核心数量
    int subReactorNum = sysconf(_SC_NPROCESSORS_ONLN);
    SlaveEpollWrap *slaveEpollWraps[subReactorNum];

    for (int i = 0; i < subReactorNum; i++) {
        SlaveEpollWrap *temp = (SlaveEpollWrap *) malloc(sizeof(SlaveEpollWrap));
        temp->epollFd = epoll_create1(0);
        slaveEpollWraps[i] = temp;
        if (slaveEpollWraps[i]->epollFd <= 0) {
            perror("sub epoll_create1 failed");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < subReactorNum; ++i) {
        pthread_t pt_fd;
        SlaveEpollWrap *slaveEpollWrap = slaveEpollWraps[i];
        if (debug) {
            printf("sub reactor fd -> %d , address %p  \n", slaveEpollWrap->epollFd, slaveEpollWrap);
        }
        if (pthread_create(&pt_fd, null, &subReactorProcessOne, slaveEpollWrap) != 0) {
            perror("create thread error");
        }
        pthread_detach(pt_fd);
    }
    ///////////////////////////////////////////////////////////////////////////////////////

    RUN_SERVER = 1;
    struct epoll_event events[MAX_EVENTS] = {0};

    for (;;) {
        //每次最多等待1000毫秒, 最多256个事件
        int num_events = epoll_wait(mainEpollWrap.epollFd, events, MAX_EVENTS, 1000);
        if (num_events <= 0) {
            continue;
        }

        for (int i = 0; i < num_events; ++i) {
            int fd = events[i].data.fd; // 获取触发事件的文件描述符
            uint32_t event_flags = events[i].events; // 获取事件标志
            if ((event_flags & EPOLLIN) && fd == mainEpollWrap.serverSocketFd) {

                /**
                 * （即监听套接字上的 accept() 事件）对应的 events[i].data.fd 是服务端的套接字（即监听套接字），
                 *  那么可读事件（已连接套接字上的数据可读事件）对应的 events[i].data.fd 应该是已连接套接字的文件描述符
                 *
                 */
                int conn_sock_fd = accept(mainEpollWrap.serverSocketFd, null, null);
                if (conn_sock_fd != -1) {
                    // 新连接已接受，接下来将其添加到 epoll 实例中，监听其可读事件（边缘触发）
                    //选择一个
                    SlaveEpollWrap *slaveEpollWrap = slaveEpollWraps[REQUEST_COUNT % subReactorNum];
                    addNewConnectionToEpoll(conn_sock_fd, slaveEpollWrap);
                    REQUEST_COUNT++;
                    continue;
                }
                if (debug) {
                    // 处理错误，如打印错误信息、关闭文件描述符等
                    perror("error >> conn_sock_fd ");
                }
            } else {
                printf("error >> the event type is not legality %d, server fd %d\n", event_flags, fd);
                RUN_SERVER = 0;
            }
        }

        if (RUN_SERVER == 0) {
            break;
        }
    }


    for (int i = 0; i < subReactorNum; ++i) {
        SlaveEpollWrap *slaveEpollWrap = slaveEpollWraps[i];
        free(slaveEpollWrap);
    }
    /**
     * 关闭epoll
     */
    if (epoll_ctl(mainEpollWrap.epollFd, EPOLL_CTL_DEL, mainEpollWrap.serverSocketFd, null) == -1) {
        perror("epoll_ctl del failed");
    }

    shutdown(mainEpollWrap.serverSocketFd, SHUT_RDWR);
    /**
     * 关闭网络套接字
     */
    close(mainEpollWrap.serverSocketFd);
}

