// probes/b5_dlopen.c
//
// 【测试目的】dlopen/dlsym/dlclose — Node.js C++ addon (napi) 兼容。
//
// 【关联项目】Bun
//
// 【上游调用链】
// Bun glibc 兼容层 & Node.js N-API addons:
//   → dlopen("libc.so", RTLD_LAZY)                              workaround-missing-symbols.cpp
//   → dlsym(handle, "getrandom") / dlsym(handle, "quick_exit")
//   → Node.js C++ napi addons → dlopen() 加载 .node 动态库
//
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    void *handle = dlopen("libc.so", RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "dlopen libc.so failed: %s", dlerror());
        return 1;
    }

    void (*sym)(const char *, ...) = dlsym(handle, "printf");
    if (!sym) {
        fprintf(stderr, "dlsym printf failed: %s", dlerror());
        dlclose(handle);
        return 1;
    }

    dlclose(handle);
    return 0;
}

