// probes/b2_rtld_next.c
//
// 【测试目的】dlsym(RTLD_NEXT) 跳转到下一符号定义 — Bun workaround-missing-symbols 核心机制。
//
// 【关联项目】Bun
//
// 【上游调用链】
// Bun glibc 兼容层符号动态查找
//   → dlsym(RTLD_NEXT, "getrandom")                           workaround-missing-symbols.cpp
//   → dlsym(RTLD_NEXT, "quick_exit")
//   → dlsym(RTLD_NEXT, "fcntl64")
//     → 动态链接器 RTLD_NEXT 语义
//
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>

int main(void) {
    void *sym = dlsym(RTLD_NEXT, "malloc");
    if (!sym) {
        fprintf(stderr, "RTLD_NEXT malloc not found: %s\n", dlerror());
        return 1;
    }
    return 0;
}

