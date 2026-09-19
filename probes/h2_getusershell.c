// probes/h2_getusershell.c
//
// 【测试目的】getusershell() 获取合法 Shell 列表。
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

typedef void (*void_fn)(void);
typedef char *(*getusershell_fn)(void);

int main(void) {
    void_fn setsh = (void_fn)dlsym(RTLD_DEFAULT, "setusershell");
    getusershell_fn getsh = (getusershell_fn)dlsym(RTLD_DEFAULT, "getusershell");
    void_fn endsh = (void_fn)dlsym(RTLD_DEFAULT, "endusershell");

    if (!setsh || !getsh || !endsh) {
        fprintf(stderr, "getusershell not in libc: %s", dlerror());
        return 1;
    }

    setsh();
    char *sh = getsh();
    endsh();

    if (!sh) {
        fprintf(stderr, "getusershell() returned NULL (empty /etc/shells or stub)");
        return 1;
    }
    return 0;
}

