// probes/j5_epoll_pwait2_fb.c
//
// 【测试目的】c13_epoll_pwait2 替代：epoll_pwait + timerfd 亚毫秒精度。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

int main(void) {
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) { fprintf(stderr, "epoll_create1: %m"); return 1; }

    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (tfd < 0) {
        fprintf(stderr, "timerfd_create: %m (fallback unusable)");
        close(epfd);
        return 1;
    }
    struct itimerspec spec;
    memset(&spec, 0, sizeof spec);
    spec.it_value.tv_nsec = 500000;
    if (timerfd_settime(tfd, 0, &spec, NULL) < 0) {
        fprintf(stderr, "timerfd_settime: %m");
        close(tfd); close(epfd); return 1;
    }
    struct epoll_event tev = { .events = EPOLLIN, .data.fd = tfd };
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, tfd, &tev) < 0) {
        fprintf(stderr, "epoll_ctl: %m");
        close(tfd); close(epfd); return 1;
    }

    struct epoll_event got;
    int n = epoll_pwait(epfd, &got, 1, 1000, NULL);
    close(tfd); close(epfd);

    if (n == 1 && got.data.fd == tfd) return 0;
    fprintf(stderr, "epoll_pwait returned %d (expected 1)", n);
    return 1;
}

