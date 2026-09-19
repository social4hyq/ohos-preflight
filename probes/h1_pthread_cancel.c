// probes/h1_pthread_cancel.c
//
// 【测试目的】pthread_cancel() 取消线程。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <errno.h>
#include <string.h>

typedef int (*pthread_cancel_fn)(unsigned long);

int main(void) {
    pthread_cancel_fn fn = (pthread_cancel_fn)dlsym(RTLD_DEFAULT, "pthread_cancel");
    if (!fn) {
        fprintf(stderr, "pthread_cancel not in libc: %s", dlerror());
        return 1;
    }
    int r = fn((unsigned long){0});
    if (r == ENOSYS) {
        fprintf(stderr, "pthread_cancel() is a no-op stub (ENOSYS)");
        return 1;
    }
    return 0;
}

