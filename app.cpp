//
// Created by cielswifts on 2023/10/28.
//

#include <cstdio>
#include <pthread.h>
#include <dlfcn.h>
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

    int debug = 1;
    if (args != NULL && args[1] != NULL) {
        debug = strtol(args[1], NULL, 10);
    }

    startLinuxEpoll(9095, debug);

    return 0;
}

