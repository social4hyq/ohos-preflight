// probes/e2_scm_rights.c
//
// 【测试目的】SCM_RIGHTS fd 传递 — libuv IPC 依赖。
//
// 【关联项目】vite-plus
//
// 【上游调用链】
// libuv Unix 域 socket fd 传递:
//   → sendmsg(fd, &msg, 0)  with SCM_RIGHTS ancillary data
//   → recvmsg(fd, &msg, 0)  receiving fd from another process
//   → libuv/src/unix/stream.c — uv_write2 通过 SCM_RIGHTS 传递 fd
// 
// vite-plus fspy fd 传递:
//   → passfd crate (polachok/passfd) — Unix socket SCM_RIGHTS
//   → vite-task/crates/fspy_shared_unix — 跨进程 fd 传递
//
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

int main(void) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        perror("socketpair");
        return 2;
    }
    int p[2];
    if (pipe(p) < 0) {
        perror("pipe");
        return 2;
    }
    int send_fd = p[0];

    struct stat st_send;
    if (fstat(send_fd, &st_send) < 0) {
        perror("fstat send");
        return 2;
    }

    char ctrl_buf[CMSG_SPACE(sizeof(int))];
    memset(ctrl_buf, 0, sizeof(ctrl_buf));
    char dummy = 'x';
    struct iovec iov = { .iov_base = &dummy, .iov_len = 1 };
    struct msghdr msg = {0};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = ctrl_buf;
    msg.msg_controllen = sizeof(ctrl_buf);
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &send_fd, sizeof(int));

    if (sendmsg(sv[0], &msg, 0) < 0) {
        perror("sendmsg");
        return 1;
    }

    char rdummy = 0;
    char rctrl[CMSG_SPACE(sizeof(int))];
    memset(rctrl, 0, sizeof(rctrl));
    struct iovec riov = { .iov_base = &rdummy, .iov_len = 1 };
    struct msghdr rmsg = {0};
    rmsg.msg_iov = &riov;
    rmsg.msg_iovlen = 1;
    rmsg.msg_control = rctrl;
    rmsg.msg_controllen = sizeof(rctrl);
    if (recvmsg(sv[1], &rmsg, 0) < 0) {
        perror("recvmsg");
        return 1;
    }
    struct cmsghdr *rcm = CMSG_FIRSTHDR(&rmsg);
    if (!rcm || rcm->cmsg_level != SOL_SOCKET || rcm->cmsg_type != SCM_RIGHTS) {
        fprintf(stderr, "no SCM_RIGHTS cmsg received\n");
        return 1;
    }
    int recv_fd = -1;
    memcpy(&recv_fd, CMSG_DATA(rcm), sizeof(int));
    if (recv_fd < 0) {
        fprintf(stderr, "recovered fd invalid\n");
        return 1;
    }

    struct stat st_recv;
    if (fstat(recv_fd, &st_recv) < 0) {
        perror("fstat recv");
        return 2;
    }
    int ok = st_send.st_dev == st_recv.st_dev && st_send.st_ino == st_recv.st_ino;
    close(recv_fd);
    close(p[0]); close(p[1]);
    close(sv[0]); close(sv[1]);
    if (!ok) {
        fprintf(stderr, "SCM_RIGHTS inode mismatch\n");
        return 1;
    }
    return 0;
}

