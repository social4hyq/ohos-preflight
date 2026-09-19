// probes/e4_epoll.c
//
// 【测试目的】epoll_create1 + epoll_wait 事件轮询。
//
// 【关联项目】Bun / WebKit
//
// 【上游调用链】
// Bun 事件循环 I/O 多路复用:
//   → linux_syscall::epoll_ctl(epfd, op, fd, event)            bun/src/sys/linux_syscall.rs:408
//     → libc::syscall(SYS_epoll_ctl, epfd, op, fd, ev)        libc crate
//       → 内核 epoll_ctl(2)
// 
// WebKit GLib 事件循环 epoll 集成:
//   → epoll_create1 / epoll_ctl / epoll_wait                      RunLoopGLib.cpp
//   → µWebSockets (uws_sys) epoll 事件驱动 (Bun 静态链接)
//
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

int main(void) {
    int efd = eventfd(0, 0);
    if (efd < 0) {
        perror("eventfd");
        return 2;
    }
    int ep = epoll_create1(EPOLL_CLOEXEC);
    if (ep < 0) {
        perror("epoll_create1");
        close(efd);
        return 1;
    }
    struct epoll_event ev = { .events = EPOLLIN, .data.fd = efd };
    if (epoll_ctl(ep, EPOLL_CTL_ADD, efd, &ev) < 0) {
        perror("epoll_ctl ADD");
        close(ep); close(efd);
        return 1;
    }
    uint64_t v = 1;
    if (write(efd, &v, sizeof(v)) != (ssize_t)sizeof(v)) {
        perror("eventfd write");
        close(ep); close(efd);
        return 2;
    }
    struct epoll_event out;
    int n = epoll_wait(ep, &out, 1, 1000);
    close(ep); close(efd);
    if (n != 1 || out.data.fd != efd) {
        fprintf(stderr, "epoll_wait returned n=%d\n", n);
        return 1;
    }
    return 0;
}

