// probes/e8_socketpair_fstat.c
//
// 【测试目的】socketpair(AF_UNIX) 创建后对每端 fd 调 fstat() 是否返 EACCES。
//
// 【关联项目】Bun
//
// 【上游调用链】
// Bun.spawn() 父进程读取 stdio:
//   → bun_sys::fstat(socketpair_fd)                              src/sys/lib.rs:2128
//     → libc::fstat(fd, &st)
//       → 内核 fstat(2)
//
// 【为何需要】fork 在 src/sys/lib.rs fstat() OHOS 分支里加了 EACCES → 返零 stat 兜底。
// 注释说明 socketpair / 某些 fd 类型在 OHOS 上 fstat 会返 EACCES。
// e1_socketpair 只测创建，本探针补测后续 fstat。
//
// Exit: 0=pass (fstat succeeds), 1=fail (fstat 返 EACCES 等错误)

#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        fprintf(stderr, "socketpair: %s", strerror(errno));
        return 1;
    }

    int rc = 0;
    for (int i = 0; i < 2; i++) {
        struct stat st;
        if (fstat(sv[i], &st) < 0) {
            fprintf(stderr, "fstat(sv[%d]): %s%s", i, strerror(errno),
                    errno == EACCES ? " — OHOS quirk reproduces, bun sys::fstat OHOS fallback needed" : "");
            rc = 1;
            break;
        }
        // sanity: S_ISSOCK should be set for a socketpair fd
        if (!S_ISSOCK(st.st_mode)) {
            fprintf(stderr, "fstat(sv[%d]) returned mode=%o, expected S_ISSOCK", i, st.st_mode);
            rc = 1;
            break;
        }
    }

    close(sv[0]);
    close(sv[1]);
    return rc;
}
