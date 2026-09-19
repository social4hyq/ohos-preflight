// probes/j9_fanotify_fb.c
//
// 【测试目的】e5_fanotify 替代：inotify 监听 IN_MODIFY 事件。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

int main(void) {
    const char *dir = getenv("HOME");
    if (!dir || !*dir) dir = "/data/storage/el2/base";
    char path[256];
    snprintf(path, sizeof path, "%s/j9_watch_%d", dir, getpid());

    int wfd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0600);
    if (wfd < 0) { fprintf(stderr, "open(%s): %m", path); return 1; }

    int ifd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (ifd < 0) {
        fprintf(stderr, "inotify_init1: %m (fallback unusable)");
        close(wfd); unlink(path);
        return 1;
    }
    int wd = inotify_add_watch(ifd, path, IN_MODIFY);
    if (wd < 0) {
        fprintf(stderr, "inotify_add_watch: %m");
        close(ifd); close(wfd); unlink(path);
        return 1;
    }

    write(wfd, "x", 1);
    fsync(wfd);

    char buf[4096];
    ssize_t n = -1;
    for (int i = 0; i < 20 && n < 0; i++) {
        n = read(ifd, buf, sizeof buf);
        if (n < 0) usleep(50 * 1000);
    }

    inotify_rm_watch(ifd, wd);
    close(ifd);
    close(wfd);
    unlink(path);

    if (n > 0 && (size_t)n >= sizeof(struct inotify_event)) return 0;
    fprintf(stderr, "no inotify event received (n=%zd): %m", n);
    return 1;
}

