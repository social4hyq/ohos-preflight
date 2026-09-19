// probes/c1_execveat_sym.c
//
// 【测试目的】execveat() 是否存在于 libc 符号表。
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
    void *sym = dlsym(RTLD_DEFAULT, "execveat");
    if (!sym) {
        fprintf(stderr, "execveat not in libc: %s\n", dlerror());
        return 1;
    }
    return 0;
}

