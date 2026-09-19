// probes/i3_pty.c
//
// 【测试目的】/dev/ptmx 伪终端设备 — Bun TTYWrap / bun-spawn 终端控制。
//
// 【关联项目】Bun
//
// 【上游调用链】
// Bun 终端控制 (TTY)
//   → open("/dev/ptmx", O_RDWR | O_NOCTTY)                   TTYWrap.cpp
//   → ioctl(fd, TIOCSCTTY, 0)                                  bun-spawn.cpp (子进程终端)
//   → ioctl(fd, TIOCGWINSZ, &ws)                               TTYWrap.cpp (终端窗口大小)
//     → 需 /dev/ptmx 可打开
//
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

int main(void) {
    int fd = open("/dev/ptmx", O_RDWR | O_NOCTTY);
    if (fd < 0) {
        fprintf(stderr, "/dev/ptmx not accessible: %s", strerror(errno));
        return 1;
    }
    close(fd);
    return 0;
}

