// probes/f3_prctl_get_name.c
//
// 【测试目的】prctl(PR_GET_NAME) — JSC ThreadingPOSIX 依赖。
//
// 【关联项目】Bun / WebKit
//
// 【上游调用链】
// JSC 线程命名 & Bun 线程管理:
//   → prctl(PR_SET_NAME, "JSC Worker", ...)                     ThreadingPOSIX.cpp:381
//   → prctl(PR_GET_NAME, buf) — 读取当前线程名称
//   → Bun Global.zig / Global.rs — prctl(PR_SET_NAME) + PR_SET_PDEATHSIG
//
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>

int main(void) {
    char name[17] = {0};
    if (prctl(PR_GET_NAME, (unsigned long)name, 0, 0, 0) < 0) {
        perror("PR_GET_NAME");
        return 1;
    }
    if (name[0] == '\0') {
        fprintf(stderr, "PR_GET_NAME returned empty\n");
        return 1;
    }
    return 0;
}

