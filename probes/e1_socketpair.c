// probes/e1_socketpair.c
//
// 【测试目的】socketpair(AF_UNIX) 创建套接字对。
//
// 【关联项目】Bun
//
// 【上游调用链】
// Bun.spawn() 父子进程 IPC 管道
//   → bun_sys::socketpair(AF_UNIX, SOCK_STREAM, 0)        bun/src/spawn_sys/spawn_process.rs:800
//     → libc::socketpair(AF_UNIX, SOCK_STREAM, 0, sv)      libc crate
//       → 内核 socketpair(2)
//
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        perror("socketpair");
        return 1;
    }
    close(sv[0]);
    close(sv[1]);
    return 0;
}

