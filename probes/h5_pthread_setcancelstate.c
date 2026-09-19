// probes/h5_pthread_setcancelstate.c
//
// 【测试目的】pthread_setcancelstate() 设置取消状态。
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

typedef int (*pthread_setcancelstate_fn)(int, int *);

int main(void) {
    pthread_setcancelstate_fn fn =
        (pthread_setcancelstate_fn)dlsym(RTLD_DEFAULT, "pthread_setcancelstate");
    if (!fn) {
        fprintf(stderr, "pthread_setcancelstate not in libc: %s", dlerror());
        return 1;
    }
    int old = 0;
    int r = fn(1 /* PTHREAD_CANCEL_DISABLE */, &old);
    if (r == ENOSYS) {
        fprintf(stderr, "pthread_setcancelstate() is a no-op stub (ENOSYS)");
        return 1;
    }
    return 0;
}

