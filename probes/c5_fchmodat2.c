// probes/c5_fchmodat2.c
//
// 【测试目的】验证 fchmodat2 (syscall 452) 可用性。
// fchmodat2 增加了 flags 参数，支持 AT_SYMLINK_NOFOLLOW 等选项，避免
// chmod 操作意外跟随符号链接修改目标文件权限。
//
// 【上游调用链】Bun（运行时验证发现）
//   Bun add / bun install 包安装流程 → 设置 node_modules 文件权限
//     → fchmodat2(AT_FDCWD, path, mode, AT_SYMLINK_NOFOLLOW)
//       → seccomp filter → SIGSYS（进程被杀死）
//
// 【影响】Bun 包管理器安装依赖时，对 node_modules 中的符号链接执行 chmod
// 操作时触发 fchmodat2。HarmonyOS seccomp 拦截该 syscall（SIGSYS），导致
// bun install 进程崩溃。降级方案：使用经典 fchmodat() 无 flags（丢失
// AT_SYMLINK_NOFOLLOW 安全语义）。
//
// Exit: 0=pass, 1=fail (SIGSYS/ENOSYS)

#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <fcntl.h>

static volatile int sigsys_caught = 0;
static void handle_sigsys(int sig, siginfo_t *info, void *ctx) {
    sigsys_caught = 1;
}

int main(void) {
    struct sigaction sa = { .sa_sigaction = handle_sigsys, .sa_flags = SA_SIGINFO };
    sigaction(SIGSYS, &sa, NULL);

    // Test with invalid path (expect ENOENT if syscall exists)
    // SYS_fchmodat2 (452) 未在 NDK 头文件中定义，使用原始编号
    long ret = syscall(452, AT_FDCWD, "/nonexistent", 0644, 0);
    if (sigsys_caught) {
        fprintf(stderr, "seccomp blocked fchmodat2 (SIGSYS)");
        return 1;
    }
    if (ret == -1 && errno == ENOSYS) {
        fprintf(stderr, "fchmodat2 not implemented");
        return 1;
    }
    return 0;
}
