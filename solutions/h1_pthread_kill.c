// solutions/h1_pthread_kill.c
//
// 替代 pthread_cancel()：使用 pthread_kill + 信号处理实现线程取消。
// OHOS musl 中 pthread_cancel 是空桩，需手动实现取消逻辑。
//
// 验证：创建线程，用 pthread_kill(SIGUSR1) + sigwait 实现取消。

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

static volatile int cancelled = 0;

static void handler(int sig) {
    cancelled = 1;
}

static void *worker(void *arg) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);

    signal(SIGUSR1, handler);
    for (int i = 0; i < 100 && !cancelled; i++) {
        usleep(10000);  // 模拟工作
    }
    return cancelled ? (void*)1 : (void*)0;
}

int main(void) {
    pthread_t tid;
    if (pthread_create(&tid, NULL, worker, NULL) != 0) {
        fprintf(stderr, "pthread_create failed: %s", strerror(errno));
        return 1;
    }
    usleep(20000);  // 让线程跑起来
    pthread_kill(tid, SIGUSR1);

    void *ret;
    pthread_join(tid, &ret);
    if (ret != (void*)1) {
        fprintf(stderr, "cancel via signal failed");
        return 1;
    }
    return 0;
}
