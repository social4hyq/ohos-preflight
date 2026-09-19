// probes/h7_cxa_thread_atexit_impl.c
//
// 【测试目的】__cxa_thread_atexit_impl — glibc 兼容层线程退出回调。
//
// 【关联项目】Bun / WebKit
//
// 【上游调用链】
//   → Bun __cxa_thread_atexit_impl — glibc 兼容层线程退出回调
//   → C++ thread_local 析构 — __cxa_thread_atexit_impl 注册析构函数
//   → WebKit ThreadingPOSIX.cpp — 线程退出清理
//
#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <dlfcn.h>
#include <string.h>

typedef int (*cxa_thread_atexit_impl_fn)(void (*func)(void*), void *obj, void *dso_symbol);

static void thread_exit_callback(void *arg) {
    *(int *)arg = 1;
}

static void *thread_func(void *arg) {
    cxa_thread_atexit_impl_fn fn =
        (cxa_thread_atexit_impl_fn)dlsym(RTLD_DEFAULT, "__cxa_thread_atexit_impl");
    if (!fn) {
        *(int *)arg = -1;
        return NULL;
    }
    int r = fn(thread_exit_callback, arg, NULL);
    if (r != 0) {
        *(int *)arg = -2;
    }
    return NULL;
}

int main(void) {
    cxa_thread_atexit_impl_fn fn =
        (cxa_thread_atexit_impl_fn)dlsym(RTLD_DEFAULT, "__cxa_thread_atexit_impl");
    if (!fn) {
        fprintf(stderr, "__cxa_thread_atexit_impl not in libc: %s", dlerror());
        return 1;
    }

    // Smoke: call on main thread, verify it doesn't crash
    int main_cb_fired = 0;
    int r = fn(thread_exit_callback, &main_cb_fired, NULL);
    if (r != 0) {
        fprintf(stderr, "__cxa_thread_atexit_impl returned %d on main thread", r);
        return 1;
    }

    // Thread exit: callback must fire when thread exits
    pthread_t tid;
    int thread_flag = 0;
    if (pthread_create(&tid, NULL, thread_func, &thread_flag) != 0) {
        fprintf(stderr, "pthread_create failed");
        return 2;
    }
    pthread_join(tid, NULL);

    if (thread_flag == -1) {
        fprintf(stderr, "__cxa_thread_atexit_impl not visible from thread dlsym");
        return 1;
    }
    if (thread_flag == -2) {
        fprintf(stderr, "__cxa_thread_atexit_impl returned non-zero in thread");
        return 1;
    }
    if (thread_flag == 0) {
        fprintf(stderr, "__cxa_thread_atexit_impl callback did not fire on thread exit");
        return 1;
    }

    return 0;
}
