// probes/h4_res_state.c
//
// 【测试目的】_res DNS 解析器全局状态。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <string.h>

int main(void) {
    void *res = dlsym(RTLD_DEFAULT, "_res");
    if (!res) {
        fprintf(stderr, "_res not in libc: %s", dlerror());
        return 1;
    }
    return 0;
}

