// probes/a2_no_new_privs.c
//
// 【测试目的】prctl(PR_SET_NO_NEW_PRIVS) 禁止提权。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/prctl.h>

int main(void) {
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
        perror("PR_SET_NO_NEW_PRIVS");
        return 1;
    }
    return 0;
}

