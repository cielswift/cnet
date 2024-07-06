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
#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <netinet/tcp.h>
#include "../json/yyjson.h"


void cnet::closeConn(cnet::ConnModel *conn, cnet::EpollModel *epollModel, int status) {
    char buffer[1]; // 创建一个小缓冲区
    memset(&buffer, 0, sizeof(buffer));
    ssize_t rn = recv(conn->getSocketFd(), buffer, 1, MSG_DONTWAIT);

    if (rn <= 0) {
        perror("closeReadDateErr->");
        fprintf(stderr, "closeReadDateErr details: %s\n", strerror(errno));
        printf("closeReadAfterNot->tid %lu,status %d,cid %d rs %zd, srid %d error %d. may be close! \n",
               pthread_self(), status, conn->getSocketFd(), rn, epollModel->getEpollFd(), errno);
    } else {
        printf("closeBefore->tid %lu,status%d,cid %d rs %zd, srid %d error %d ,receive data: %s \n",
               pthread_self(), status, conn->getSocketFd(), rn, epollModel->getEpollFd(), errno, buffer);
    }

    struct epoll_event event{0};
    memset(&event, 0, sizeof(event));
    if (epoll_ctl(epollModel->getEpollFd(), EPOLL_CTL_DEL, conn->getSocketFd(), &event) == -1) {
        perror("del >> del client fd failed");
        printf("err >> del client fd failed tid %lu,\n", pthread_self());
    }

    shutdown(conn->getSocketFd(), SHUT_RDWR);
    //关闭网络套接字
    if (close(conn->getSocketFd()) == -1) {
        perror("close >> client fd failed");
        printf("err >> close client fd failed tid %lu,\n", pthread_self());
    }

    int connFd = conn->getSocketFd();

    auto it = epollModel->getSocketFds()->find(connFd);
    if (it != epollModel->getSocketFds()->end()) {
        // 在删除之前保存被删除的元素
        auto *connVal = it->second;
        delete connVal;
        epollModel->getSocketFds()->erase(connFd);
        printf("closeAfter->tid %lu,status%d,cid %d rs %zd, srid %d error %d ,finish \n",
               pthread_self(), status, connFd, rn, epollModel->getEpollFd(), errno);
    }

}

void cnet::readData(cnet::ConnModel *conn, cnet::EpollModel *epollModel, int status) {
    char buffer_receive[512];
    memset(&buffer_receive, 0, sizeof(buffer_receive));
    // 可读事件：已连接套接字有数据可读，可以调用 recv() 或类似函数
    ssize_t rn = recv(conn->getSocketFd(), buffer_receive, 512, MSG_DONTWAIT);

    if (rn <= 0) {
        perror("readDateErr->");
        fprintf(stderr, "readDateErr details: %s\n", strerror(errno));
        printf("readAfterNot->tid %lu,status %d,cid %d rs %zd, srid %d error %d. may be close! \n",
               pthread_self(), status, conn->getSocketFd(), rn, epollModel->getEpollFd(), errno);
        return;
    }


    printf("readAfter->tid %lu,status %d,cid %d rs %zd, srid %d error %d ,receive data: %s \n",
           pthread_self(), status, conn->getSocketFd(), rn, epollModel->getEpollFd(), errno, buffer_receive);

    time_t now;
    time(&now);
    char text[32];
    int realCount = 0;
    memset(text, '\0', sizeof(text));
    sprintf(text, "{\"time\":%ld}\n", now);
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

    ssize_t wn = send(conn->getSocketFd(), rep, realCount2, MSG_DONTWAIT);
    if (wn <= 0) {
        perror("sendErr->");
        fprintf(stderr, "sendErr details: %s\n", strerror(errno));
    }

}


void printClient(cnet::EpollModel *epollModel) {
    for (const auto &item: *epollModel->getSocketFds()) {
        printf("printIng->tid %lu,srid %d,cid %d \n", pthread_self(), epollModel->getEpollFd(),
               item.second->getSocketFd());
    }
}

void cnet::subReactorLogic(cnet::EpollServer *epollServer, cnet::EpollModel *epollModel) {

    if (epollServer == nullptr || epollModel == nullptr) {
        printf("ERROR: %p or %p null \n", epollServer, epollModel);
        return;
    }

    printf("thread id %lu,sub reactor %d start\n", pthread_self(), epollModel->getEpollFd());

    for (;;) {

        if (epollServer->getStatus() == 2) {
            for (const auto &item: *epollModel->getSocketFds()) {
                //关闭epoll
                struct epoll_event event{0};
                memset(&event, 0, sizeof(event));
                if (epoll_ctl(epollModel->getEpollFd(), EPOLL_CTL_DEL, item.second->getSocketFd(), &event) == -1) {
                    perror("epoll>>_ctl del failed");
                    printf("err >> epoll_ctl del failed tid %lu,\n", pthread_self());
                }
                //关闭网络套接字
                shutdown(item.second->getSocketFd(), SHUT_RDWR);
                close(item.second->getSocketFd());
            }

            break;
        }


        //printClient(epollModel);

        struct epoll_event events[epollServer->getMaxEvent()];
        memset(events, 0, sizeof(events));

        int num_events = epoll_wait(epollModel->getEpollFd(), events, epollServer->getMaxEvent(), 1000);
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
                cnet::ConnModel *connection = epollModel->getSocketFds()->find(conn_fd)->second;
                cnet::readData(connection, epollModel, 1);
                cnet::closeConn(connection, epollModel, 1);
                continue;
            }

            if ((conn_event_flags & EPOLLIN) && (conn_event_flags & EPOLLRDHUP)) {
                cnet::ConnModel *connection = epollModel->getSocketFds()->find(conn_fd)->second;
                cnet::readData(connection, epollModel, 2);
                cnet::closeConn(connection, epollModel, 2);
                continue;
            }

            //异常关闭
            if ((conn_event_flags & EPOLLHUP)) {
                cnet::ConnModel *connection = epollModel->getSocketFds()->find(conn_fd)->second;
                cnet::closeConn(connection, epollModel, 3);
                continue;
            }

            if ((conn_event_flags & EPOLLRDHUP)) {
                cnet::ConnModel *connection = epollModel->getSocketFds()->find(conn_fd)->second;
                cnet::closeConn(connection, epollModel, 4);
                continue;
            }

            if ((conn_event_flags & EPOLLERR)) {
                cnet::ConnModel *connection = epollModel->getSocketFds()->find(conn_fd)->second;
                cnet::closeConn(connection, epollModel, 5);
                continue;
            }

            //正常读取
            if ((conn_event_flags & EPOLLIN)) {
                cnet::ConnModel *connection = epollModel->getSocketFds()->find(conn_fd)->second;
                cnet::readData(connection, epollModel, 6);
                continue;
            }

            if ((conn_event_flags & EPOLLOUT)) {
                //可写 忽略
                continue;
            }

            printf("warning>>> tid %lu,cid %d srid %d\n", pthread_self(), conn_fd, epollModel->getEpollFd());
        }
    }
}


void cnet::addNewConnectionToEpoll(const cnet::ConnModel &connModel, cnet::EpollModel *epollModel) {
    /**
     * 返回文件描述符的 状态
    * 这些标志通常是一些宏定义的位掩码，如 O_RDONLY、O_WRONLY、O_RDWR、O_NONBLOCK 等，
     * 可以使用按位与（&）操作符检查特定标志是否已设置
    */
    int flags = fcntl(connModel.getSocketFd(), F_GETFL, 0);
    /**
     * 设置文件描述符 为非阻塞，因为边缘触发模式通常配合非阻塞 I/O 使用
     */
    fcntl(connModel.getSocketFd(), F_SETFL, flags | O_NONBLOCK);

    struct epoll_event sub_conn_event{0};
    memset(&sub_conn_event, 0, sizeof(sub_conn_event));

    //设置缓冲区大小
    int bufferSize = 1024 * 64; //64kb
    if (setsockopt(connModel.getSocketFd(), SOL_SOCKET, SO_RCVBUF, &bufferSize, sizeof(bufferSize)) < 0) {
        perror("ERROR:SETTING RECEIVE BUFFER SIZE");
        close(connModel.getSocketFd());
        exit(EXIT_FAILURE);
    }
    if (setsockopt(connModel.getSocketFd(), SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize)) < 0) {
        perror("ERROR:SETTING SEND BUFFER SIZE");
        close(connModel.getSocketFd());
        exit(EXIT_FAILURE);
    }
    //设置超时
    struct timeval tv{0};
    // 设置发送超时
    tv.tv_sec = 2000 / 1000; // 2秒
    tv.tv_usec = (2000 % 1000) * 1000; // 微秒
    if (setsockopt(connModel.getSocketFd(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt SO_SNDTIMEO");
        close(connModel.getSocketFd());
        exit(EXIT_FAILURE);
    }
    if (setsockopt(connModel.getSocketFd(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt SO_RCVTIMEO");
        close(connModel.getSocketFd());
        exit(EXIT_FAILURE);
    }

    // 设置SO_KEEPALIVE及其相关参数，例如空闲时间设为2小时，探测间隔为15秒，最大探测次数为5次
    int val = 1; // 启用SO_KEEPALIVE选项
    // 首先启用SO_KEEPALIVE选项
    if (setsockopt(connModel.getSocketFd(), SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val)) < 0) {
        perror("setsockopt SO_KEEPALIVE");
        exit(EXIT_FAILURE);
    }

    // 下面的设置可能依赖于平台和内核版本，不一定所有系统都支持
    int idle = 7200;
    if (idle > 0 && setsockopt(connModel.getSocketFd(), IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) < 0) {
        perror("setsockopt TCP_KEEPIDLE");
        exit(EXIT_FAILURE);
    }

    int interval = 15;
    if (interval > 0 &&
        setsockopt(connModel.getSocketFd(), IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval)) < 0) {
        perror("setsockopt TCP_KEEPINTVL");
        exit(EXIT_FAILURE);
    }

    int count = 5;
    if (count > 0 && setsockopt(connModel.getSocketFd(), IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count)) < 0) {
        perror("setsockopt TCP_KEEPCNT");
        exit(EXIT_FAILURE);
    }


    /**
     * 监听可读事件 和 挂起事件 并启用边缘触发
     *  EPOLLHUP 连接关闭 当连接断开时，epoll_wait()函数会返回EPOLLHUP事件。同时，由于TCP连接关闭时，TCP栈会在接收缓冲区填充一个 EOF 标记，
     *  EPOLLERR 连接异常
     *
     *  这时 EPOLLIN 事件也会触发
     */
    sub_conn_event.events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLRDHUP | EPOLLET;
    sub_conn_event.data.fd = connModel.getSocketFd(); // 关联已连接套接字的文件描述符

    if (epoll_ctl(epollModel->getEpollFd(), EPOLL_CTL_ADD, connModel.getSocketFd(), &sub_conn_event) == -1) {
        perror("ERROR:epoll_ctl err>");
    }

    std::unordered_map < int, cnet::ConnModel * > *subs = epollModel->getSocketFds();
    auto *con = new cnet::ConnModel;
    con->setSocketFd(connModel.getSocketFd());
    con->setRemoteAddr(connModel.getRemoteAddr());
    con->setRemotePort(connModel.getRemotePort());
    subs->insert({connModel.getSocketFd(), con});

    printf("thread id %lu. new connection fd %d. sub epoll fd %d. remote ip %s. remote port %d. regis finish \n",
           pthread_self(),
           connModel.getSocketFd(),
           epollModel->getEpollFd(),
           con->getRemoteAddr().c_str(),
           con->getRemotePort());

    printf("this sub epoll regis %zu \n",subs->size());
}


void cnet::mainReactorLogic(cnet::EpollServer *epollServer) {

    cnet::EpollModel *mainReactor = epollServer->getMainReactor();
    std::unordered_map < int, cnet::EpollModel * > *subReactors = epollServer->getSubReactors();

    if (mainReactor == nullptr || subReactors == nullptr) {
        printf("ERROR: %p or %p null \n", mainReactor, subReactors);
        return;
    }

    long allRequests = 0;
    cnet::EpollModel *subReactorsArr[subReactors->size()];
    for (const auto &item: *subReactors) {
        subReactorsArr[allRequests] = item.second;
        allRequests++;
    }

    allRequests = 0;

    int serverFd = mainReactor->getSocketFds()->begin()->first;
    int meFd = mainReactor->getEpollFd();
    printf("thread id %lu,main reactor %d server fd %d. start\n", pthread_self(), meFd, serverFd);

    for (;;) {
        if (epollServer->getStatus() == 2) {
            struct epoll_event event{0};
            memset(&event, 0, sizeof(event));
            if (epoll_ctl(meFd, EPOLL_CTL_DEL, serverFd, &event) == -1) {
                perror("ERROR:ctl del failed");
            }

            /**
             * 关闭网络套接字
             */
            shutdown(serverFd, SHUT_RDWR);
            close(serverFd);
            close(meFd);
            break;
        }


        //每次最多等待1000毫秒, 最多256个事件
        struct epoll_event events[epollServer->getMaxEvent()];
        memset(&events, 0, sizeof(events));
        int num_events = epoll_wait(mainReactor->getEpollFd(), events, epollServer->getMaxEvent(), 1000);
        if (num_events <= 0) {
            continue;
        }

        std::vector <ConnModel> connsModels(0);

        for (int i = 0; i < num_events; ++i) {
            int fd = events[i].data.fd; // 获取触发事件的文件描述符
            uint32_t event_flags = events[i].events; // 获取事件标志
            if ((event_flags & EPOLLIN) && fd == serverFd) {

                /**
                 *  即监听套接字上的 accept() 事件,对应的 events[i].data.fd 是服务端的套接字（即监听套接字），
                 *   那么可读事件（已连接套接字上的数据可读事件）对应的 events[i].data.fd 应该是已连接套接字的文件描述符
                 */
                struct sockaddr_in client_addr{0};
                memset(&client_addr, 0, sizeof(client_addr));
                socklen_t addr_len = sizeof(struct sockaddr_in);
                int conn_sock_fd = accept(serverFd, (struct sockaddr *) &client_addr, &addr_len);

                bool hadConned = false;
                for (const auto &item: *subReactors) {
                    std::unordered_map < int, cnet::ConnModel * > *conns = item.second->getSocketFds();
                    if (conns != nullptr) {
                        if (conns->find(conn_sock_fd) != conns->end()) {
                            hadConned = true;
                        }
                    }
                }

                // 打印客户端的地址和端口
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
                int port = ntohs(client_addr.sin_port);

                ConnModel connModel;
                connModel.setSocketFd(conn_sock_fd);
                connModel.setRemoteAddr(std::string(client_ip));
                connModel.setRemotePort(port);
                connsModels.push_back(connModel);

            } else {
                printf("error >> the event type is not legality %d, server fd %d\n", event_flags, fd);
            }
        }

        cnet::EpollModel *selectModel = subReactorsArr[allRequests % subReactors->size()];
        for (const auto &item: connsModels) {
            addNewConnectionToEpoll(item, selectModel);
        }
    }

}

int cnet::createSocketServer(int port) {
    /**
     * AF_INET：用于 Internet 协议版本 4 (IPv4)，这是最常见的选择，用于基于 TCP/IP 的网络通信
     * SOCK_STREAM :tcp , SOCK_DGRAM:udp
     * 在大多数情况下，可以将其设置为 0
     *
     *   返回服务器端套接字描述符
     */
    int server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd < 0) {
        perror("ERROR:CREATE SERVER SOCKET DEFEAT");
        exit(EXIT_FAILURE);
    }

    // 假设之前已经设置了SO_REUSEADDR，现在要关闭它
    int optval = 0; // 0 表示关闭SO_REUSEADDR
    // 使用setsockopt关闭SO_REUSEADDR选项
    if (setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("ERROR:setsockopt to disable SO_REUSEADDR failed");
        exit(EXIT_FAILURE);
    }

    //设置超时
    struct timeval tv{0};
    tv.tv_sec = 2000 / 1000; // 2秒
    tv.tv_usec = (2000 % 1000) * 1000; // 微秒
    if (setsockopt(server_socket_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        perror("ERROR:SO_SNDTIMEO");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(server_socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("ERROR:SO_RCVTIMEO");
        exit(EXIT_FAILURE);
    }

    /**
    * 由于 server_addr 通常用于存放服务器的网络地址信息（如 IP 地址、端口号等），清零操作确保了结构体内部的所有字段都处于已知的初始状态
    * 这一行代码的作用就是将整个 server_addr 结构体的所有字节清零
    */
    struct sockaddr_in server_addr{0}; // 服务器地址结构
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    /**
     * 不同类型的套接字可能需要不同的地址结构体，例如IPv4套接字使用sockaddr_in结构体，IPv6套接字使用sockaddr_in6结构体。
     * 这些结构体的大小并不相同，因此，bind()函数需要知道实际使用的结构体的确切大小
     */
    if (bind(server_socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("ERROR:to bind server socket");
        exit(EXIT_FAILURE);
    }

    /**
     * 最多允许1024个未完成连接排队
     */
    if (listen(server_socket_fd, 1024) < 0) {
        perror("ERROR:to listen on server socket");
        exit(EXIT_FAILURE);
    }

    return server_socket_fd;
}


int cnet::createEpoll(int socketFd) {
    int epoll_fd = epoll_create1(0); // epoll_create1在较新的Linux内核中更为推荐
    if (epoll_fd <= 0) {
        perror("ERROR:create1 failed");
        exit(EXIT_FAILURE);
    }

    struct epoll_event event{0};
    memset(&event, 0, sizeof(event));

    event.events = EPOLLIN | EPOLLET; // 监听可读事件，并使用边缘触发模式（水平触发请去掉EPOLLET）
    event.data.fd = socketFd; // 要监听的套接字文件描述符

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socketFd, &event) == -1) {
        perror("ERROR:ctl add failed");
        exit(EXIT_FAILURE);
    }
    return epoll_fd;
}


int cnet::EpollServer::getPort() const {
    return port;
}

int cnet::EpollServer::getSubReactor() const {
    return subReactor;
}


int cnet::EpollServer::getMaxConnection() const {
    return maxConnection;
}

void cnet::EpollServer::setMaxConnection(int maxConnection) {
    EpollServer::maxConnection = maxConnection;
}

bool cnet::EpollServer::isDebug() const {
    return debug;
}

void cnet::EpollServer::setDebug(bool debug) {
    EpollServer::debug = debug;
}

cnet::EpollServer::EpollServer(int port, int subReactor, int maxConnection, bool debug, int maxEvent) : port(port),
                                                                                                        subReactor(
                                                                                                                subReactor),
                                                                                                        maxConnection(
                                                                                                                maxConnection),
                                                                                                        debug(debug),
                                                                                                        maxEvent(
                                                                                                                maxEvent) {
    this->mainReactor = nullptr;
    this->subReactors = nullptr;
    this->status = 0;
}


bool cnet::EpollServer::startLinuxEpoll() {
    int socketFd = createSocketServer(this->port);
    int epollFd = createEpoll(socketFd);

    {
        auto *socketFds = new std::unordered_map<int, ConnModel *>(2);
        auto *connModel = new cnet::ConnModel();
        connModel->setRemoteAddr("127.0.0.1");
        connModel->setRemotePort(this->port);
        connModel->setSocketFd(socketFd);
        socketFds->insert({socketFd, connModel});
        this->mainReactor = new cnet::EpollModel(socketFds, epollFd, true);
        //start
        this->mainReactor->start(this, epollFd);
    }

    {
        this->subReactors = new std::unordered_map<int, cnet::EpollModel *>(this->subReactor);
        for (int i = 0; i < this->subReactor; ++i) {
            int epoll_fd = epoll_create1(0);
            if (epoll_fd <= 0) {
                perror("ERROR:create1 failed");
                exit(EXIT_FAILURE);
            }
            auto *socketFds = new std::unordered_map<int, ConnModel *>(this->maxConnection);
            auto *mode = new cnet::EpollModel(socketFds, epoll_fd, false);
            this->subReactors->insert({epoll_fd, mode});
        }

        for (const auto &item: *this->subReactors) {
            //start
            item.second->start(this, item.first);
        }
    }

    this->status = 1;
    return true;
}


bool cnet::EpollModel::start(cnet::EpollServer *epollServer, int epollFd) {
    if (this->main) {
        auto *myThread = new std::thread(&mainReactorLogic, epollServer);
        setThread(myThread);
        myThread->detach();
        return true;
    } else {
        std::unordered_map < int, cnet::EpollModel * > *reactors = epollServer->getSubReactors();
        auto *myThread = new std::thread(&subReactorLogic, epollServer, reactors->find(epollFd)->second);
        setThread(myThread);
        myThread->detach();
        return true;
    }
}

bool cnet::EpollModel::stop(cnet::EpollServer *epollServer) {

}

bool cnet::EpollServer::showDownServer() {
    this->status = 2;
    return true;
}

int cnet::EpollServer::getMaxEvent() const {
    return maxEvent;
}

void cnet::EpollServer::setMaxEvent(int maxEvent) {
    EpollServer::maxEvent = maxEvent;
}

cnet::EpollModel *cnet::EpollServer::getMainReactor() const {
    return mainReactor;
}

std::unordered_map<int, cnet::EpollModel *> *cnet::EpollServer::getSubReactors() const {
    return subReactors;
}

int cnet::EpollServer::getStatus() const {
    return status;
}

int cnet::ConnModel::getSocketFd() const {
    return socketFd;
}

void cnet::ConnModel::setSocketFd(int socketFd) {
    ConnModel::socketFd = socketFd;
}

const std::string &cnet::ConnModel::getRemoteAddr() const {
    return remoteAddr;
}

void cnet::ConnModel::setRemoteAddr(const std::string &remoteAddr) {
    ConnModel::remoteAddr = remoteAddr;
}

int cnet::ConnModel::getRemotePort() const {
    return remotePort;
}

void cnet::ConnModel::setRemotePort(int remotePort) {
    ConnModel::remotePort = remotePort;
}


cnet::EpollModel::EpollModel(std::unordered_map<int, cnet::ConnModel *> *socketFds,
                             int epollFd, bool main)
        : socketFds(socketFds),
          epollFd(epollFd),
          main(main) {
    this->thread = nullptr;
}

int cnet::EpollModel::getEpollFd() const {
    return epollFd;
}

std::unordered_map<int, cnet::ConnModel *> *cnet::EpollModel::getSocketFds() const {
    return socketFds;
}

bool cnet::EpollModel::isMain() const {
    return main;
}

std::thread *cnet::EpollModel::getThread() const {
    return thread;
}

void cnet::EpollModel::setThread(std::thread *thread) {
    EpollModel::thread = thread;
}


