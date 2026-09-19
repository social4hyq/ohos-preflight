// 替代：epoll_pwait（毫秒精度）+ timerfd 补齐纳秒级唤醒
// timerfd 注册到同一个 epoll，到期触发 epoll 立即返回
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

int epoll_pwait2_fallback(int epfd, struct epoll_event *evs, int max,
                          const struct timespec *ts,
                          const sigset_t *sigmask) {
    if (!ts) return epoll_pwait(epfd, evs, max, -1, sigmask);

    // 纳秒精度场景：用 timerfd 替代 timeout 参数
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    struct itimerspec spec = { .it_value = *ts };
    timerfd_settime(tfd, 0, &spec, NULL);
    struct epoll_event tev = { .events = EPOLLIN, .data.fd = tfd };
    epoll_ctl(epfd, EPOLL_CTL_ADD, tfd, &tev);

    int n = epoll_pwait(epfd, evs, max, -1, sigmask);
    epoll_ctl(epfd, EPOLL_CTL_DEL, tfd, NULL);
    close(tfd);

    // 过滤掉 timerfd 自身的事件
    int out = 0;
    for (int i = 0; i < n; i++)
        if (evs[i].data.fd != tfd) evs[out++] = evs[i];
    return out;
}
// TODO: 待 HarmonyOS 放行 epoll_pwait2 (441) 后改用
//   syscall(SYS_epoll_pwait2, epfd, evs, max, ts, sigmask, sizeof(sigset_t))
