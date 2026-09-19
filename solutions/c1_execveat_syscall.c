// solutions/c1_execveat_syscall.c
//
// 替代方案：当 libc 未导出 execveat() 时，用 syscall() 直接调用。
// OHOS musl libc 中 execveat 符号缺失，但内核支持该系统调用。
//
// 验证方式：用 syscall(__NR_execveat, ...) 执行 /bin/true 测试。

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

int main(void) {
    char *argv[] = {"/bin/true", NULL};
    char *envp[] = {NULL};

    long ret = syscall(SYS_execveat, AT_FDCWD, "/bin/true", argv, envp, 0);
    // SYS_execveat 成功不返回；失败返回 -1
    if (ret == -1 && errno == ENOSYS) {
        fprintf(stderr, "SYS_execveat not available: %s", strerror(errno));
        return 1;
    }
    // 其他错误（如权限）也算"syscall 可用"
    fprintf(stderr, "SYS_execveat available but exec failed: %s", strerror(errno));
    return 0;
}
