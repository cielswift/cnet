//
// Created by cielswifts on 2024/4/21.
//

#ifndef LINUXDEV_EPOLLSERVER_H
#define LINUXDEV_EPOLLSERVER_H


#include <vector>
#include <unordered_map>
#include <thread>

namespace cnet {

    //向前声明
    class EpollServer;

    class ConnModel;

    class EpollModel;

    class ConnModel {
    private:
        int socketFd;
        std::string remoteAddr;
        int remotePort;
    public:
        int getSocketFd() const;

        void setSocketFd(int socketFd);

        const std::string &getRemoteAddr() const;

        void setRemoteAddr(const std::string &remoteAddr);

        int getRemotePort() const;

        void setRemotePort(int remotePort);
    };

    class EpollModel {
    private:
        std::thread *thread{};
        std::unordered_map<int, cnet::ConnModel *> *socketFds;
        int epollFd;
        bool main;
    public:
        explicit EpollModel(std::unordered_map<int, ConnModel *> *socketFds, int epollFd, bool main);

        int getEpollFd() const;

        std::unordered_map<int, cnet::ConnModel *> *getSocketFds() const;

        bool isMain() const;

        bool start(cnet::EpollServer *epollServer, int epollFd);

        bool stop(cnet::EpollServer *epollServer);
    };

    class EpollServer {
    private:
        int port;
        int subReactor;
        int maxConnection;
        int maxEvent;
        bool debug;
        //0 init 1 run 2 stop
        int status;
        cnet::EpollModel *mainReactor;
        std::unordered_map<int, cnet::EpollModel *> *subReactors;
    public:
        int getPort() const;

        int getSubReactor() const;

        int getMaxConnection() const;

        void setMaxConnection(int maxConnection);

        bool isDebug() const;

        void setDebug(bool debug);

        explicit EpollServer(int port, int subReactor, int maxConnection, bool debug, int maxEvent);

        int getMaxEvent() const;

        void setMaxEvent(int maxEvent);

        EpollModel *getMainReactor() const;

        std::unordered_map<int, cnet::EpollModel *> *getSubReactors() const;

        int getStatus() const;

        bool startLinuxEpoll();

        bool showDownServer();

        //删除拷贝构造函数
        EpollServer(const EpollServer &other) = delete;

        //删除拷贝赋值运算符
        EpollServer &operator=(const EpollServer &other) = delete;
    };

    int createSocketServer(int port);

    int createEpoll(int socketFd);

    void mainReactorLogic(cnet::EpollServer *epollServer);

    void addNewConnectionToEpoll(const cnet::ConnModel& connModel, cnet::EpollModel *epollModel);

    void subReactorLogic(cnet::EpollServer *epollServer, cnet::EpollModel *epollModel);

    void readData(cnet::ConnModel* conn,cnet::EpollModel *epollModel,int status);

    void closeConn(cnet::ConnModel* conn,cnet::EpollModel *epollModel,int status);
}


#endif //LINUXDEV_EPOLLSERVER_H
