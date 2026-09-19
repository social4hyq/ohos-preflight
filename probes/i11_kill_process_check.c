// probes/i11_kill_process_check.c
//
// 【测试目的】验证 kill(pid, 0) 进程存在性检查可用性。
// kill(pid, 0) 不发送信号，仅检查调用者是否有权限向目标进程发信号，
// 是进程监控的基础机制。
//
// 【上游调用链】Playwright
//   Playwright 浏览器进程监控与关闭:
//     → processLauncher.ts — process.kill(browserPid, 0) 检查浏览器是否存活
//     → processLauncher.ts — process.kill(browserPid, 'SIGTERM') 优雅关闭
//     → processLauncher.ts — process.kill(browserPid, 'SIGKILL') 强制终止
//     → Node.js process.kill() → libuv uv_kill() → kill(2)
//
// 【影响】Playwright 通过 kill(pid, 0) 监控浏览器进程存活状态，
// 通过 kill(pid, SIGTERM/SIGKILL) 关闭浏览器。若 kill() 被沙箱限制，
// Playwright 无法检测浏览器崩溃，也无法正常关闭浏览器进程。
//
// Exit: 0=pass, 1=fail

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

int main(void) {
    // kill(pid, 0) — process existence check (signal=0 means no signal sent)
    if (kill(getpid(), 0) < 0) {
        fprintf(stderr, "kill(getpid(), 0) failed: %s", strerror(errno));
        return 1;
    }

    // kill(pid, SIGCONT) — harmless signal to verify signal sending works.
    // SIGCONT is safe: it just continues a stopped process; if not stopped, it's a no-op.
    if (kill(getpid(), SIGCONT) < 0) {
        fprintf(stderr, "kill(getpid(), SIGCONT) failed: %s", strerror(errno));
        return 1;
    }

    return 0;
}
