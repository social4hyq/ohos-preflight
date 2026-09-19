// probes/e6_flock.c
//
// 【测试目的】flock(LOCK_EX|LOCK_NB) 获取文件顾问锁并释放。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <errno.h>
#include <unistd.h>

int main(void) {
    const char *dir = getenv("TMPDIR");
    if (!dir) dir = ".";

    char tmpl[256];
    snprintf(tmpl, sizeof(tmpl), "%s/ohos-preflight-flock-XXXXXX", dir);
    int fd = mkstemp(tmpl);
    if (fd < 0) {
        fprintf(stderr, "mkstemp failed: %s\n", strerror(errno));
        return 1;
    }

    if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
        fprintf(stderr, "flock(LOCK_EX) failed: %s\n", strerror(errno));
        unlink(tmpl);
        close(fd);
        return 1;
    }

    if (flock(fd, LOCK_UN) < 0) {
        fprintf(stderr, "flock(LOCK_UN) failed: %s\n", strerror(errno));
        unlink(tmpl);
        close(fd);
        return 1;
    }

    unlink(tmpl);
    close(fd);
    return 0;
}

