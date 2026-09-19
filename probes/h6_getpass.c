// probes/h6_getpass.c
//
// 【测试目的】getpass() 读取密码（不回声）。
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

typedef char *(*getpass_fn)(const char *);

int main(void) {
    getpass_fn fn = (getpass_fn)dlsym(RTLD_DEFAULT, "getpass");
    if (!fn) {
        fprintf(stderr, "getpass not in libc: %s", dlerror());
        return 1;
    }
    // Don't actually call getpass() — it reads from /dev/tty which
    // may hang in non-interactive contexts. Symbol existence is enough.
    return 0;
}

