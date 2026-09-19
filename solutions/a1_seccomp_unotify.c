// 替代：用 LD_PRELOAD shim 拦截 libc 函数（无法拦截 raw syscall）
// 适用场景：监控/审计应用对 libc 文件、网络、进程接口的调用
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>

typedef int (*orig_open_t)(const char *, int, ...);
int open(const char *path, int flags, ...) {
    static orig_open_t real = NULL;
    if (!real) real = (orig_open_t)dlsym(RTLD_NEXT, "open");
    fprintf(stderr, "[shim] open(%s, %d)\n", path, flags);
    // 自定义策略：拒绝 / 重写路径 / 记录审计日志
    return real(path, flags);
}
// 编译：clang -shared -fPIC shim.c -o libshim.so -ldl
// 运行：LD_PRELOAD=./libshim.so ./target_app
// 局限：无法拦截直接 syscall(...)；如需 raw syscall 拦截只能等放行
// TODO: 待 HarmonyOS 放行 seccomp(SECCOMP_SET_MODE_FILTER) 后改用标准 seccomp-unotify 方案
