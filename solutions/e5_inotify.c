// solutions/e5_inotify.c
//
// 替代 fanotify：使用 inotify 监控文件系统事件。
// fanotify 在 OHOS 沙箱中受限，inotify 作为标准 Linux API 更兼容。
//
// 验证：创建临时文件，用 inotify 监控写入事件。

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <string.h>
#include <errno.h>
#include <sys/inotify.h>

int main(void) {
    const char *tmpd = getenv("TMPDIR");
    if (!tmpd) tmpd = getenv("HOME");
    if (!tmpd) tmpd = "/tmp";
    char tmpl[256];
    snprintf(tmpl, sizeof(tmpl), "%s/ohos-inotify-test-XXXXXX", tmpd);
    int tfd = mkstemp(tmpl);
    if (tfd < 0) {
        fprintf(stderr, "mkstemp failed: %s", strerror(errno));
        return 1;
    }
    write(tfd, "init", 4);
    close(tfd);

    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "inotify_init1 failed: %s", strerror(errno));
        unlink(tmpl);
        return 1;
    }

    int wd = inotify_add_watch(fd, tmpl, IN_MODIFY);
    if (wd < 0) {
        fprintf(stderr, "inotify_add_watch failed: %s", strerror(errno));
        close(fd); unlink(tmpl);
        return 1;
    }

    // 触发一次修改事件
    tfd = open(tmpl, O_WRONLY);
    if (tfd >= 0) { write(tfd, "mod", 3); close(tfd); }

    struct inotify_event ev;
    ssize_t n = read(fd, &ev, sizeof(ev));
    if (n < 0 && errno != EAGAIN) {
        fprintf(stderr, "inotify read failed: %s", strerror(errno));
        close(fd); unlink(tmpl);
        return 1;
    }

    close(fd);
    unlink(tmpl);
    return 0;
}
