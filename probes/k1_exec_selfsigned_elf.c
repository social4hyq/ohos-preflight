// probes/k1_exec_selfsigned_elf.c
//
// 【测试目的】验证一个"刚刚落地、尚未经过运行时签名"的 ELF 能否被 exec()。
// 与 l0/03_raw_elf_exec.sh（原地执行已构建好的探针）不同：本探针把自身
// 字节复制到一个新 inode（模拟 tar 解包 / bun install 刚落盘的产物），
// 再 fork+execve 那份拷贝 —— 这正是 ohos-bun 的 ensure_signed() 介入之前
// 的状态。
//
// 【关联项目】ohos-bun
//
// 【上游调用链】
// bun install 解包依赖 / bun build --compile 产物落盘:
//   → 新文件从 tar/网络写入磁盘（无 codesign 修改）
//     → 首次 exec 该文件
//       → 内核签名策略拒绝 / 或 devmode 放行
//         → ensure_signed() 兜底重签         spawn_sys/spawn_process.rs
//
// 【为何需要】当前所有实测结论都是在 devmode=on 下跑出来的；本探针是
// 判断"关闭 devmode 后 ensure_signed() 这层兜底是否仍然必需"的直接证据。
//
// Exit: 0=pass (拷贝可以直接 exec，无需运行时重签)
//       1=fail (EACCES/EPERM/ENOEXEC 或崩溃 — 仍需要 ensure_signed 兜底)
//       2=unsupported (拿不到 /proc/self/exe 或没有可写 tmpdir)

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHILD_FLAG "--k1-child"

static int copy_file(const char *src, const char *dst) {
    int in = open(src, O_RDONLY);
    if (in < 0) return -1;
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if (out < 0) { close(in); return -1; }
    char buf[65536];
    ssize_t n;
    int ok = 1;
    while ((n = read(in, buf, sizeof(buf))) > 0) {
        if (write(out, buf, (size_t)n) != n) { ok = 0; break; }
    }
    if (n < 0) ok = 0;
    close(in);
    close(out);
    return ok ? 0 : -1;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], CHILD_FLAG) == 0) {
        // Just being able to reach this line proves exec succeeded.
        return 0;
    }

    char self_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
    if (len < 0) {
        fprintf(stderr, "readlink(/proc/self/exe): %s", strerror(errno));
        return 2;
    }
    self_path[len] = '\0';

    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || !*tmpdir) tmpdir = "/data/storage/el2/base/tmp";
    char copy_path[PATH_MAX];
    snprintf(copy_path, sizeof(copy_path), "%s/.ohos-preflight-k1-%d", tmpdir, getpid());

    if (copy_file(self_path, copy_path) != 0) {
        fprintf(stderr, "failed to stage a fresh copy at %s: %s", copy_path, strerror(errno));
        return 2;
    }

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "fork: %s", strerror(errno));
        unlink(copy_path);
        return 2;
    }
    if (pid == 0) {
        char *child_argv[] = { copy_path, (char *)CHILD_FLAG, NULL };
        execv(copy_path, child_argv);
        // execv only returns on failure.
        _exit(111 + (errno == EACCES ? 1 : errno == EPERM ? 2 : 0));
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "waitpid: %s", strerror(errno));
        unlink(copy_path);
        return 2;
    }
    unlink(copy_path);

    if (WIFSIGNALED(status)) {
        fprintf(stderr, "exec of freshly-copied ELF was killed by signal %d", WTERMSIG(status));
        return 1;
    }
    if (!WIFEXITED(status)) {
        fprintf(stderr, "exec of freshly-copied ELF ended abnormally (raw status %d)", status);
        return 1;
    }
    int code = WEXITSTATUS(status);
    if (code == 0) return 0;
    if (code >= 111) {
        fprintf(stderr, "execv refused freshly-copied ELF: %s",
                code == 112 ? "EACCES" : code == 113 ? "EPERM" : "exec failed");
        return 1;
    }
    fprintf(stderr, "child exited with unexpected status %d", code);
    return 1;
}
