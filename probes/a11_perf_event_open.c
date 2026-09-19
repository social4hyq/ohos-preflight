// probes/a11_perf_event_open.c
//
// 【测试目的】perf_event_open (241) 性能监控 — JSC 性能分析。
//
// 【关联项目】Bun运行
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>

static volatile int sigsys_caught = 0;
static void handle_sigsys(int sig, siginfo_t *info, void *ctx) {
    sigsys_caught = 1;
}

int main(void) {
    struct sigaction sa = { .sa_sigaction = handle_sigsys, .sa_flags = SA_SIGINFO };
    sigaction(SIGSYS, &sa, NULL);

    struct perf_event_attr attr = {0};
    attr.type = PERF_TYPE_SOFTWARE;
    attr.size = sizeof(attr);
    attr.config = PERF_COUNT_SW_CPU_CLOCK;
    attr.disabled = 1;

    long ret = syscall(SYS_perf_event_open, &attr, 0, -1, -1, 0);
    if (sigsys_caught) {
        fprintf(stderr, "seccomp blocked perf_event_open (SIGSYS)");
        return 1;
    }
    if (ret == -1 && errno == ENOSYS) {
        fprintf(stderr, "perf_event_open not implemented");
        return 1;
    }
    return 0;
}

