//
// Created by cielswifts on 2023/10/28.
//

#include <cstdio>
#include <pthread.h>
#include <clocale>
#include <csignal>
#include <cstdlib>
#include <unistd.h>
#include "the_epoll_net/EpollServer.h"

void printArgs(int argCount, char *args[]) {
    printf("argCount>> %d\n", argCount);
    for (int i = 0; i < argCount; ++i) {
        printf("args>> %s\n", args[i]);
    }
}
    
void handle(int sig) {
    printf("kill >> %d\n", sig);
    exit(sig);
}


int main(int argCount, char *args[]) {
    setlocale(LC_ALL, "zh_CN.UTF-8");
    signal(SIGINT, &handle);
    signal(SIGTERM, &handle);
    signal(SIGKILL, &handle);

    printArgs(argCount, args);

    printf("now pid is %d \n", getpid());

    printf("now tid is %lu \n", pthread_self());

    cnet::EpollServer epollServer(22095,2,1024, true,1024);
    epollServer.startLinuxEpoll();

    while (true) {
        std::this_thread::sleep_for(std::chrono::hours(1)); // 使线程睡眠1小时
    }
    return 0;
}

