// probes/i10_pr_set_pdeathsig.c
//
// 【测试目的】验证 prctl(PR_SET_PDEATHSIG, SIGKILL) 可用性。
// PR_SET_PDEATHSIG 确保父进程死亡时内核自动向子进程发送指定信号。
// 这是进程管理的关键安全机制：防止浏览器子进程成为孤儿进程。
//
// 【上游调用链】Playwright
//   playwright-core/src/server/processLauncher.ts
//     → Node.js child_process.spawn() 启动浏览器子进程
//       → prctl(PR_SET_PDEATHSIG, SIGKILL, 0, 0, 0)
//         → 父进程异常退出 → 内核自动 SIGKILL 子进程
//         → HarmonyOS 沙箱可能拒绝（EPERM 或返回 0 但无效）
//
// 【影响】Playwright 启动 Chromium/Firefox/WebKit 浏览器子进程后，
// 若父进程（Node.js）崩溃或被 kill，浏览器子进程应自动终止。
// 若 PR_SET_PDEATHSIG 不可用，浏览器进程将成为孤儿继续运行，
// 占用内存和端口，影响后续测试会话。
//
// Exit: 0=pass (prctl accepted), 1=fail (EPERM/EINVAL)

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/prctl.h>
#include <signal.h>

int main(void) {
    int ret = prctl(PR_SET_PDEATHSIG, SIGKILL, 0, 0, 0);
    if (ret < 0) {
        fprintf(stderr, "prctl(PR_SET_PDEATHSIG): %s", strerror(errno));
        return 1;
    }
    // Verify by reading back
    int sig = 0;
    if (prctl(PR_GET_PDEATHSIG, &sig) < 0) {
        fprintf(stderr, "prctl(PR_GET_PDEATHSIG): %s", strerror(errno));
        return 1;
    }
    if (sig != SIGKILL) {
        fprintf(stderr, "PR_SET_PDEATHSIG accepted but PR_GET_PDEATHSIG returned %d instead of %d",
                sig, SIGKILL);
        return 1;
    }
    return 0;
}
