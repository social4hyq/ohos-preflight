// probes/b3_rtld_default.c
//
// 【测试目的】dlsym(RTLD_DEFAULT) 全局符号搜索。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>

int main(void) {
    void *sym = dlsym(RTLD_DEFAULT, "open");
    if (!sym) {
        fprintf(stderr, "RTLD_DEFAULT open not found: %s\n", dlerror());
        return 1;
    }
    return 0;
}

