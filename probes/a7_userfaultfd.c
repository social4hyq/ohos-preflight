// probes/a7_userfaultfd.c
//
// 【测试目的】userfaultfd 用户态缺页处理。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef SYS_userfaultfd
#define SYS_userfaultfd 282
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC 02000000
#endif

int main(void) {
    long ret = syscall(SYS_userfaultfd, O_CLOEXEC);
    if (ret < 0) {
        perror("userfaultfd");
        return 1;
    }
    close((int)ret);
    return 0;
}

