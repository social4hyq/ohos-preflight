// probes/j2_close_range_fb.c
//
// 【测试目的】a10_close_range 替代：/proc/self/fd 枚举关闭。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#define _GNU_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int close_range_fallback(unsigned int low, unsigned int high) {
    DIR *d = opendir("/proc/self/fd");
    if (!d) return -1;
    int self_dirfd = dirfd(d);
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        unsigned int fd = (unsigned int)atoi(e->d_name);
        if (fd >= low && fd <= high && (int)fd != self_dirfd) close(fd);
    }
    closedir(d);
    return 0;
}

int main(void) {
    int a = open("/dev/null", O_RDONLY);
    int b = open("/dev/null", O_RDONLY);
    int c = open("/dev/null", O_RDONLY);
    if (a < 0 || b < 0 || c < 0) {
        fprintf(stderr, "open /dev/null failed");
        return 1;
    }
    unsigned int lo = a, hi = c;

    if (close_range_fallback(lo, hi) != 0) {
        fprintf(stderr, "fallback returned error");
        return 1;
    }
    int still_open = 0;
    if (fcntl(a, F_GETFD) != -1) still_open++;
    if (fcntl(b, F_GETFD) != -1) still_open++;
    if (fcntl(c, F_GETFD) != -1) still_open++;
    if (still_open) {
        fprintf(stderr, "%d fd still open after fallback", still_open);
        return 1;
    }
    return 0;
}

