// probes/e3_eventfd_signalfd.c
//
// 【测试目的】eventfd + signalfd — libuv 事件循环依赖。
//
// 【关联项目】Bun / WebKit
//
// 【上游调用链】
// libuv & WebKit 事件通知机制 (Bun 静态链接 libuv):
//   → eventfd(0, EFD_NONBLOCK|EFD_CLOEXEC)                    RealTimeThreads.cpp (WebKit)
//   → signalfd(-1, &mask, SFD_NONBLOCK|SFD_CLOEXEC)            libuv/src/unix/linux.c
//   → timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK|TFD_CLOEXEC) RunLoopGLib.cpp (WebKit)
//   → libuv epoll 事件循环注册 eventfd/signalfd/timerfd
//
#define _GNU_SOURCE
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/eventfd.h>
#include <sys/signalfd.h>
#include <unistd.h>

int main(void) {
    int efd = eventfd(0, 0);
    if (efd < 0) {
        perror("eventfd");
        return 1;
    }
    uint64_t v = 7;
    if (write(efd, &v, sizeof(v)) != (ssize_t)sizeof(v)) {
        perror("eventfd write");
        close(efd);
        return 2;
    }
    uint64_t r = 0;
    if (read(efd, &r, sizeof(r)) != (ssize_t)sizeof(r) || r != 7) {
        perror("eventfd read");
        close(efd);
        return 1;
    }
    close(efd);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    int sfd = signalfd(-1, &mask, 0);
    if (sfd < 0) {
        perror("signalfd");
        return 1;
    }
    close(sfd);
    return 0;
}

