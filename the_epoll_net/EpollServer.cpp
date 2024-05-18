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
#include <netinet/in.h>

#define null NULL

static char RUN_SERVER = 0;
static unsigned long long REQUEST_COUNT = 0;
static const int MAX_EVENTS = 256;
static const int MAX_CLIENT_SIZE = 1024;
static int debug = 1;

struct SlaveEpollWrap {
    int epollFd;
    int socketFds[MAX_CLIENT_SIZE];
};

struct MainEpollWrap {
    int epollFd;
    int serverSocketFd;
};


void closeClient(int status, int fd, SlaveEpollWrap *pSlaveEpollWrap) {
    char buffer[1]; // 创建一个小缓冲区
    memset(&buffer, 0, sizeof(buffer));
    ssize_t rn = recv(fd, buffer, 1, MSG_DONTWAIT);
    perror("closeClientErr->");
    fprintf(stderr, "closeClientErr details: %s\n", strerror(errno));

    if (debug) {
        printf("closeBefore->tid %lu,status%d,cid %d rs %zd, srid %d error %d ,receive data: %s \n",
               pthread_self(), status, fd, rn, pSlaveEpollWrap->epollFd, errno, buffer);
    }

    struct epoll_event event{0};
    memset(&event, 0, sizeof(event));
    if (epoll_ctl(pSlaveEpollWrap->epollFd, EPOLL_CTL_DEL, fd, &event) == -1) {
        perror("del >> client fd del failed");
        printf("err >> del client fd del failed tid %lu \n", pthread_self());
    }

    shutdown(fd, SHUT_RDWR);
    //关闭网络套接字
    if (close(fd) == -1) {
        perror("close >> client fd failed");
        printf("err >> close client fd failed tid %lu,\n", pthread_self());
    }

    for (int i = 0; i < MAX_CLIENT_SIZE; ++i) {
        int client_fd = pSlaveEpollWrap->socketFds[i];
        if (client_fd == fd) {
            pSlaveEpollWrap->socketFds[i] = 0;
            if (debug) {
                printf("closeAfter->tid %lu,status%d,cid %d rs %zd, srid %d error %d ,finish \n",
                       pthread_self(), status, fd, rn, pSlaveEpollWrap->epollFd, errno);
            }
            break;
        }
    }
}
    
void readDate(int fd, int status, unsigned int isWrite, SlaveEpollWrap *pSlaveEpollWrap) {
    char buffer_receive[512];
    memset(&buffer_receive, 0, sizeof(buffer_receive));
    // 可读事件：已连接套接字有数据可读，可以调用 recv() 或类似函数
    ssize_t rn = recv(fd, buffer_receive, 512, MSG_DONTWAIT);
    perror("readDateErr->");
    fprintf(stderr, "readDateErr details: %s\n", strerror(errno));

    if (rn > 0) {
        if (debug) {
            // printf("readAfter->tid %lu,status %d,cid %d rs %zd, srid %d error %d ,receive data: %s \n",
            //       pthread_self(), status, fd, rn, pSlaveEpollWrap->epollFd, errno, buffer_receive);
        }

        time_t now;
        time(&now);
        char text[32];
        int realCount = 0;
        memset(text, '\0', sizeof(text));
        sprintf(text, "{\"time\":%lld}\n", now);
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

        if (isWrite) {
            ssize_t wn = send(fd, rep, realCount2, MSG_DONTWAIT);
            perror("sendErr->");
            fprintf(stderr, "sendErr details: %s\n", strerror(errno));
            if (debug) {
                // printf("writeAfter->tid %lu,status %d,cid %d ws %zd, srid %d error %d\n",
                //        pthread_self(), status, fd, wn, pSlaveEpollWrap->epollFd, errno);
            }
        } else {
            if (debug) {
                //  printf("writeAfter->tid %lu,status %d,cid %d ws %d, srid %d error %d\n",
                //       pthread_self(), status, fd, 0, pSlaveEpollWrap->epollFd, errno);
            }
        }

    } else {
        if (debug) {
            //  printf("readAfterNot->tid %lu,status %d,cid %d rs %zd, srid %d error %d. may be close! \n",
            //      pthread_self(), status, fd, rn, pSlaveEpollWrap->epollFd, errno);
        }
    }
}


void printClient(SlaveEpollWrap *pSlaveEpollWrap) {
    for (int i = 0; i < MAX_CLIENT_SIZE; ++i) {
        if (pSlaveEpollWrap->socketFds[i] != 0) {
            printf("printIng->tid %lu,srid %d,cid %d \n", pthread_self(), pSlaveEpollWrap->epollFd,
                   pSlaveEpollWrap->socketFds[i]);
        }
    }
}

time_t getNow() {
    time_t now;
    time(&now);
    return now;
}


void *subReactorProcessOne(void *params) {
    if (params == null) {
        if (debug) {
            printf("error params is null ! \n");
        }
        return params;
    }

    unsigned long long SUB_PROCESS_COUNT = 0;

    SlaveEpollWrap *pSlaveEpollWrap = (SlaveEpollWrap *) params;
    printf("tid %lu,sub reactor start process epoll fd %d, address %p \n", pthread_self(), pSlaveEpollWrap->epollFd,
           params);

    for (;;) {
        SUB_PROCESS_COUNT++;
        if (SUB_PROCESS_COUNT % (60) == 0) {
            printClient(pSlaveEpollWrap);
        }

        struct epoll_event events[MAX_EVENTS] = {0};
        memset(events, 0, sizeof(events));

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
                readDate(conn_fd, 1, (conn_event_flags & EPOLLOUT), pSlaveEpollWrap);
                closeClient(1, conn_fd, pSlaveEpollWrap);
                continue;
            }

            if ((conn_event_flags & EPOLLIN) && (conn_event_flags & EPOLLRDHUP)) {
                readDate(conn_fd, 2, (conn_event_flags & EPOLLOUT), pSlaveEpollWrap);
                closeClient(2, conn_fd, pSlaveEpollWrap);
                continue;
            }

            //异常关闭
            if ((conn_event_flags & EPOLLHUP)) {
                closeClient(3, conn_fd, pSlaveEpollWrap);
                continue;
            }

            if ((conn_event_flags & EPOLLRDHUP)) {
                closeClient(4, conn_fd, pSlaveEpollWrap);
                continue;
            }

            if ((conn_event_flags & EPOLLERR)) {
                closeClient(5, conn_fd, pSlaveEpollWrap);
                continue;
            }

            //正常读取
            if ((conn_event_flags & EPOLLIN)) {
                readDate(conn_fd, 6, (conn_event_flags & EPOLLOUT), pSlaveEpollWrap);
                continue;
            }

            if ((conn_event_flags & EPOLLOUT)) {
                if (debug) {
                    printf("writeReady->tid %lu,cid %d srid %d \n",
                           pthread_self(), conn_fd, pSlaveEpollWrap->epollFd, errno);
                }
                continue;
            }

            printf("tid %lu,cid %d srid %d\n", pthread_self(), conn_fd, pSlaveEpollWrap->epollFd);
        }

        if (RUN_SERVER == 0) {
            break;
        }
    }


    for (int i = 0; i < MAX_CLIENT_SIZE; ++i) {
        int client_fd = pSlaveEpollWrap->socketFds[i];
        if (client_fd != 0) {
            //关闭epoll
            struct epoll_event event{0};
            memset(&event, 0, sizeof(event));
            if (epoll_ctl(pSlaveEpollWrap->epollFd, EPOLL_CTL_DEL, client_fd, &event) == -1) {
                perror("epoll>>_ctl del failed");
                printf("err >> epoll_ctl del failed tid %lu,\n", pthread_self());
            }
            //关闭网络套接字
            shutdown(client_fd, SHUT_RDWR);
            close(client_fd);
        }
    }

    close(pSlaveEpollWrap->epollFd);
    if (debug) {
        printf("tid %lu, sub reactor end process epoll fd %d \n", pthread_self(), pSlaveEpollWrap->epollFd);
    }
    return null;
}



////////////////////////////////////////////////////////////////////////////////////////


int hasLeftover(SlaveEpollWrap *subReactors) {
    for (int i = 0; i < MAX_CLIENT_SIZE; ++i) {
        if (subReactors->socketFds[i] == 0) {
            return 1;
        }
    }
    return 0;
}

int hadConned(int fd, SlaveEpollWrap *subReactors[], int len) {
    for (int i = 0; i < len; ++i) {
        SlaveEpollWrap *st = (subReactors[i]);
        for (int j = 0; j < MAX_CLIENT_SIZE; ++j) {
            if (st->socketFds[i] == fd) {
                return st->epollFd;
            }
        }
    }
    return 0;
}

void addNewConnectionToEpoll(int conn_sock_fd, SlaveEpollWrap *subReactors) {
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
    memset(&sub_conn_event, 0, sizeof(sub_conn_event));

    /**
     * 监听可读事件 和 挂起事件 并启用边缘触发
     *  EPOLLHUP 连接关闭 当连接断开时，epoll_wait()函数会返回EPOLLHUP事件。同时，由于TCP连接关闭时，TCP栈会在接收缓冲区填充一个 EOF 标记，
     *  EPOLLERR 连接异常
     *
     *  这时 EPOLLIN 事件也会触发
     */
    sub_conn_event.events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLRDHUP | EPOLLET;
    sub_conn_event.data.fd = conn_sock_fd; // 关联已连接套接字的文件描述符

    if (epoll_ctl(subReactors->epollFd, EPOLL_CTL_ADD, conn_sock_fd, &sub_conn_event) == -1) {
        // 处理错误，如打印错误信息、关闭文件描述符等
        perror("sub>>_epoll_ctl err");
        printf("err >> sub_epoll_ctl err tid %lu\n", pthread_self());
    }

    for (int i = 0; i < MAX_CLIENT_SIZE; ++i) {
        if (subReactors->socketFds[i] == 0) {
            subReactors->socketFds[i] = conn_sock_fd;
            if (debug) {
                printf("newConn->tid %lu. cid %d. srid %d regis finish \n", pthread_self(), conn_sock_fd,
                       subReactors->epollFd);
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
    * 这一行代码的作用就是将整个 server_addr 结构体的所有字节清零
    */
    memset(&server_addr, 0, sizeof(server_addr));
    /**
     * AF_INET：用于 Internet 协议版本 4 (IPv4)，这是最常见的选择，用于基于 TCP/IP 的网络通信
     * SOCK_STREAM :tcp , SOCK_DGRAM:udp
     * 在大多数情况下，可以将其设置为 0
     *
     */
    server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd < 0) {
        perror("Failed >> to create server socket");
        printf("err >> Failed to create server socket tid %lu\n", pthread_self());
        exit(EXIT_FAILURE);
    }

    // 假设之前已经设置了SO_REUSEADDR，现在要关闭它
    int optval = 0; // 0 表示关闭SO_REUSEADDR
    socklen_t len = sizeof(optval);
    // 使用setsockopt关闭SO_REUSEADDR选项
    if (setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, len) < 0) {
        perror("setsockopt >> to disable SO_REUSEADDR failed");
        printf("err >> setsockopt to disable SO_REUSEADDR failed tid %lu\n", pthread_self());
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /**
     * 不同类型的套接字可能需要不同的地址结构体，例如IPv4套接字使用sockaddr_in结构体，IPv6套接字使用sockaddr_in6结构体。
     * 这些结构体的大小并不相同，因此，bind()函数需要知道实际使用的结构体的确切大小
     */
    if (bind(server_socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("Failed >>  to bind server socket");
        printf("err >> Failed to bind server socket tid %lu\n", pthread_self());
        exit(EXIT_FAILURE);
    }

    /**
     * 最多允许1024个未完成连接排队
     */
    if (listen(server_socket_fd, 1024) < 0) {
        perror("Failed >> to listen on server socket");
        printf("err >> Failed to listen on server socket tid %lu\n", pthread_self());
        exit(EXIT_FAILURE);
    }

    return server_socket_fd;
}


int createEpoll(int server_socket_fd) {
    int epoll_fd = epoll_create1(0); // epoll_create1在较新的Linux内核中更为推荐
    if (epoll_fd <= 0) {
        perror("epoll >> _create1 failed");
        printf("err >> epoll_create1 failed tid %lu\n", pthread_self());
        exit(EXIT_FAILURE);
    }

    struct epoll_event event{0};
    memset(&event, 0, sizeof(event));

    event.events = EPOLLIN | EPOLLET; // 监听可读事件，并使用边缘触发模式（水平触发请去掉EPOLLET）
    event.data.fd = server_socket_fd; // 要监听的套接字文件描述符

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket_fd, &event) == -1) {
        perror("epoll >> _ctl add failed");
        printf("err >> epoll_ctl add failed tid %lu\n", pthread_self());
        exit(EXIT_FAILURE);
    }
    return epoll_fd;
}


void startLinuxEpoll(int port, int deBug) {
    debug = deBug;
    int server_socket_fd = createSocketServer(port);
    int epoll_fd = createEpoll(server_socket_fd);
    MainEpollWrap mainEpollWrap = {epoll_fd, server_socket_fd};
    printf("severFd %d, mainEpollFd %d \n", mainEpollWrap.serverSocketFd, mainEpollWrap.epollFd);

    //从reactor 根据可用的核心数量
    int subReactorNum = 2;//sysconf(_SC_NPROCESSORS_ONLN);
    SlaveEpollWrap *slaveEpollWraps[subReactorNum];

    for (int i = 0; i < subReactorNum; i++) {
        SlaveEpollWrap *temp = (SlaveEpollWrap *) malloc(sizeof(SlaveEpollWrap));
        temp->epollFd = epoll_create1(0);
        slaveEpollWraps[i] = temp;
        if (slaveEpollWraps[i]->epollFd <= 0) {
            perror("sub >> epoll_create1 failed");
            printf("err >> sub epoll_create1 failed tid %lu\n", pthread_self());
            exit(EXIT_FAILURE);
        }
    }

    pthread_t pt_fds[subReactorNum];
    for (int i = 0; i < subReactorNum; ++i) {
        if (pthread_create(&pt_fds[i], null, &subReactorProcessOne, slaveEpollWraps[i]) != 0) {
            perror("create >> thread error");
            printf("err >> create thread error tid %lu\n", pthread_self());
        }
        pthread_detach(pt_fds[i]);
    }   
    ///////////////////////////////////////////////////////////////////////////////////////

    RUN_SERVER = 1;

    for (;;) {
        //每次最多等待1000毫秒, 最多256个事件
        struct epoll_event events[MAX_EVENTS] = {0};
        memset(&events, 0, sizeof(events));

        int num_events = epoll_wait(mainEpollWrap.epollFd, events, MAX_EVENTS, 1000);
        if (num_events <= 0) {
            continue;
        }

        int conn_sock_fds[num_events];
        memset(&conn_sock_fds, 0, sizeof(conn_sock_fds));

        for (int i = 0; i < num_events; ++i) {
            int fd = events[i].data.fd; // 获取触发事件的文件描述符
            uint32_t event_flags = events[i].events; // 获取事件标志
            if ((event_flags & EPOLLIN) && fd == mainEpollWrap.serverSocketFd) {
                
                /**
                 *  即监听套接字上的 accept() 事件,对应的 events[i].data.fd 是服务端的套接字（即监听套接字），
                 *      那么可读事件（已连接套接字上的数据可读事件）对应的 events[i].data.fd 应该是已连接套接字的文件描述符
                 */
                struct sockaddr_in client_addr{0};
                memset(&client_addr, 0, sizeof(client_addr));
                socklen_t addr_len = sizeof(struct sockaddr_in);
                int conn_sock_fd = accept(mainEpollWrap.serverSocketFd, (struct sockaddr *) &client_addr, &addr_len);

                int hadConnFd = hadConned(conn_sock_fd, slaveEpollWraps, subReactorNum);
                // 打印客户端的地址和端口
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);

                if (debug) {
                    printf("newConn->tid %lu. cid %d. cidIp %s. cidPort %d. error %d. hadConnSRFd %d\n",
                           pthread_self(), conn_sock_fd, client_ip,
                           ntohs(client_addr.sin_port), errno, hadConnFd);
                }

                conn_sock_fds[i] = conn_sock_fd;
            } else {
                printf("error >> the event type is not legality %d, server fd %d\n", event_flags, fd);
                RUN_SERVER = 0;
            }
        }

        for (int i = 0; i < num_events; ++i) {
            if (conn_sock_fds[i] == 0) {
                continue;
            }

            SlaveEpollWrap *slaveEpollWrap = slaveEpollWraps[REQUEST_COUNT % subReactorNum];
            REQUEST_COUNT++;
            // 新连接已接受，接下来将其添加到 sub epoll 实例中，监听其可读事件（边缘触发）
            int leftover = hasLeftover(slaveEpollWrap);
            if (!leftover) {
                printf("error >> connection client is full. this conn wile be drop  fd %d\n", conn_sock_fds[i]);
                continue;
            }

            addNewConnectionToEpoll(conn_sock_fds[i], slaveEpollWrap);
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
    struct epoll_event event{0};
    memset(&event, 0, sizeof(event));
    if (epoll_ctl(mainEpollWrap.epollFd, EPOLL_CTL_DEL, mainEpollWrap.serverSocketFd, &event) == -1) {
        perror("epoll >> _ctl del failed");
        printf("err >> epoll_ctl del failed tid %lu\n", pthread_self());
    }

    /**
     * 关闭网络套接字
     */
    shutdown(mainEpollWrap.serverSocketFd, SHUT_RDWR);
    close(mainEpollWrap.serverSocketFd);
    close(mainEpollWrap.epollFd);
}

