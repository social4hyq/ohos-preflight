// probes/j10_pthread_cancel_fb.c
//
// 【测试目的】h1_pthread_cancel 替代：pthread_kill + 信号 handler + volatile flag。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#define _GNU_SOURCE
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static volatile sig_atomic_t cancelled = 0;

static void on_cancel(int sig) { (void)sig; cancelled = 1; }

static void *worker(void *arg) {
    (void)arg;
    struct sigaction sa = { .sa_handler = on_cancel };
    sigaction(SIGUSR1, &sa, NULL);
    while (!cancelled) {
        struct timespec ts = { 0, 1000000 };
        nanosleep(&ts, NULL);
    }
    return (void *)0x42;
}

int main(void) {
    pthread_t t;
    if (pthread_create(&t, NULL, worker, NULL) != 0) {
        fprintf(stderr, "pthread_create failed");
        return 1;
    }
    struct timespec slack = { 0, 50 * 1000 * 1000 };
    nanosleep(&slack, NULL);

    if (pthread_kill(t, SIGUSR1) != 0) {
        fprintf(stderr, "pthread_kill failed");
        return 1;
    }

    void *ret = NULL;
    if (pthread_join(t, &ret) != 0) {
        fprintf(stderr, "pthread_join failed");
        return 1;
    }
    if (ret == (void *)0x42 && cancelled) return 0;
    fprintf(stderr, "worker did not cancel cleanly (ret=%p cancelled=%d)",
            ret, cancelled);
    return 1;
}

