// probes/k5_open_oexec_xbit.c
//
// 【测试目的】验证 open(path, O_EXEC) 是否正确校验文件的可执行权限位。
//
// 【关联项目】ohos-bun
//
// 【上游调用链】
// is_executable_file()（判断一个路径是否可以被 exec）:
//   → #if defined(__OHOS__): 改用 stat()+S_ISREG+access(X_OK)   c-bindings.cpp:55-64
//   → 原本的 open(path, O_EXEC|...) 路径被跳过，因为:
//     "OHOS kernel bug: open(O_EXEC) doesn't check file x permission
//      bit, so a 0660 file incorrectly succeeds."
//
// 【为何需要】这条降级从未有独立探针验证过是否还在复现。
//
// Exit: 0=pass (0660 文件的 open(O_EXEC) 正确返回 EACCES — 内核 bug 已修复，
//              is_executable_file() 可以恢复用 O_EXEC 而不必 stat+access 绕行)
//       1=fail (0660 文件的 open(O_EXEC) 仍然"成功" — bug 仍然复现)
//       2=unsupported (拿不到可写 tmpdir，或 O_EXEC 未定义)

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef O_EXEC
int main(void) {
    fprintf(stderr, "O_EXEC not defined by this libc");
    return 2;
}
#else
int main(void) {
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || !*tmpdir) tmpdir = "/data/storage/el2/base/tmp";

    char path[512];
    snprintf(path, sizeof(path), "%s/.ohos-preflight-k5-%d", tmpdir, getpid());

    // 0660: readable/writable, deliberately NOT executable by anyone.
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0660);
    if (fd < 0) {
        fprintf(stderr, "open(O_CREAT) staging file: %s", strerror(errno));
        return 2;
    }
    close(fd);

    int exec_fd = open(path, O_EXEC | O_CLOEXEC | O_NONBLOCK, 0);
    int saved_errno = errno;
    if (exec_fd >= 0) close(exec_fd);
    unlink(path);

    if (exec_fd >= 0) {
        fprintf(stderr, "open(path, O_EXEC) succeeded on a 0660 (no x bit) file -- kernel bug reproduces");
        return 1;
    }
    if (saved_errno != EACCES) {
        fprintf(stderr, "open(path, O_EXEC) failed but with unexpected errno: %s", strerror(saved_errno));
        return 2;
    }
    return 0;
}
#endif
