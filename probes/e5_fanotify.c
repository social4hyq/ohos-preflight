// probes/e5_fanotify.c
//
// 【测试目的】fanotify_init 文件系统事件监控。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/fanotify.h>
#include <unistd.h>

int main(void) {
    int fd = fanotify_init(FAN_CLASS_NOTIF, O_RDONLY | O_LARGEFILE);
    if (fd < 0) {
        perror("fanotify_init");
        return (errno == ENOSYS || errno == EPERM) ? 1 : 2;
    }
    close(fd);
    return 0;
}

