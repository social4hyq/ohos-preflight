// probes/g2_dladdr.c
//
// 【测试目的】dladdr() 地址反查所属共享库。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>

void self_func(void) {}

int main(void) {
    Dl_info info;
    if (dladdr((void *)self_func, &info) == 0) {
        fprintf(stderr, "dladdr() could not resolve self_func");
        return 1;
    }
    if (!info.dli_fname || !info.dli_fname[0]) {
        fprintf(stderr, "dladdr() returned empty path");
        return 1;
    }
    return 0;
}

