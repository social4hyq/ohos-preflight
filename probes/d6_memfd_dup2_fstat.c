// probes/d6_memfd_dup2_fstat.c
//
// 【测试目的】验证 memfd_create + 子进程 dup2(memfd, 1) + write，
// 父进程 waitpid 后 fstat memfd 能取回写入的字节数。
//
// 【关联项目】Bun
//
// 【上游调用链】
// Bun.spawnSync() stdio 采集（替代 socketpair，零拷贝读取）:
//   → bun_sys::memfd_create("bun-spawn-stdio", CLOEXEC|SEAL)    spawn_sys/spawn_process.rs:782
//     → posix_spawn 子进程 dup2(memfd, STDOUT_FILENO)
//       → 子进程 write(1, "...", n)
//         → 父 waitpid + fstat(memfd, &st) → st.st_size == n
//
// 【为何需要】fork 提交 0028 "Disable memfd for spawnSync on OHOS" 的根因：
// OHOS 上 fstat(memfd) 返回 st_size=0，即使子进程已经写入数据。
// 补丁兜底为 socketpair。本探针验证该 quirk 是否仍复现。
//
// Exit: 0=pass (st_size == bytes written), 1=fail (st_size=0 quirk reproduces)

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef SYS_memfd_create
#define SYS_memfd_create 279
#endif
#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif
#ifndef MFD_ALLOW_SEALING
#define MFD_ALLOW_SEALING 0x0002U
#endif

static const char PAYLOAD[] = "ohos-memfd-probe-payload-1234567890";

int main(void) {
    int memfd = (int)syscall(SYS_memfd_create, "bun-spawn-stdio",
                             MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (memfd < 0) {
        fprintf(stderr, "memfd_create: %s", strerror(errno));
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "fork: %s", strerror(errno));
        close(memfd);
        return 1;
    }
    if (pid == 0) {
        if (dup2(memfd, STDOUT_FILENO) < 0) _exit(11);
        ssize_t n = write(STDOUT_FILENO, PAYLOAD, sizeof(PAYLOAD) - 1);
        if (n != (ssize_t)(sizeof(PAYLOAD) - 1)) _exit(12);
        _exit(0);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "waitpid: %s", strerror(errno));
        close(memfd);
        return 1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "child exit status: %d", status);
        close(memfd);
        return 1;
    }

    struct stat st;
    if (fstat(memfd, &st) < 0) {
        fprintf(stderr, "fstat(memfd): %s", strerror(errno));
        close(memfd);
        return 1;
    }
    close(memfd);

    if (st.st_size == 0) {
        fprintf(stderr, "OHOS memfd fstat quirk reproduces: st_size=0 (expected %zu)",
                sizeof(PAYLOAD) - 1);
        return 1;
    }
    if (st.st_size != (off_t)(sizeof(PAYLOAD) - 1)) {
        fprintf(stderr, "fstat st_size=%lld != expected %zu",
                (long long)st.st_size, sizeof(PAYLOAD) - 1);
        return 1;
    }
    return 0;
}
