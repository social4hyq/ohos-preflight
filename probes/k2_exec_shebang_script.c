// probes/k2_exec_shebang_script.c
//
// 【测试目的】验证内核 binfmt_script 是否会展开直接 execve() 的 `#!` 脚本
// （而不是先交给 shell 解释）。刻意用 execve()（不是 system()/popen()），
// 因为后两者会在用户态自己解析 shebang，掩盖内核层面的行为。
//
// 【关联项目】ohos-bun
//
// 【上游调用链】
// bun run <script-with-shebang> / package.json bin 字段直接 exec:
//   → execve(script_path, argv, envp)
//     → 内核 binfmt_script 应展开为 execve(interpreter, [interpreter, script_path, ...])
//       → 若内核拒绝（EPERM/EACCES/ENOEXEC）
//         → spawn_process.rs 手工解析 shebang 兜底         spawn_sys/spawn_process.rs:989-1094
//
// 【为何需要】OHOS_TEST_STATUS.md 记录 run-extensionless.test.ts 的
// shebang exec 兜底"为什么没生效"未查透 —— 本探针直接测内核路径本身
//是否可用，与手工兜底逻辑解耦。
//
// Exit: 0=pass (内核 binfmt_script 展开成功，脚本正常运行)
//       1=fail (EACCES/EPERM/ENOEXEC，或子进程状态不对 — 仍需要手工兜底)
//       2=unsupported (没有可写 tmpdir)

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

int main(void) {
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || !*tmpdir) tmpdir = "/data/storage/el2/base/tmp";

    char script_path[512];
    snprintf(script_path, sizeof(script_path), "%s/.ohos-preflight-k2-%d.sh", tmpdir, getpid());

    int fd = open(script_path, O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if (fd < 0) {
        fprintf(stderr, "open(%s): %s", script_path, strerror(errno));
        return 2;
    }
    static const char body[] = "#!/bin/sh\nprintf 'shebang-ok'\n";
    if (write(fd, body, sizeof(body) - 1) != (ssize_t)(sizeof(body) - 1)) {
        fprintf(stderr, "write script body: %s", strerror(errno));
        close(fd);
        unlink(script_path);
        return 2;
    }
    close(fd);

    int outpipe[2];
    if (pipe(outpipe) != 0) {
        fprintf(stderr, "pipe: %s", strerror(errno));
        unlink(script_path);
        return 2;
    }

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "fork: %s", strerror(errno));
        unlink(script_path);
        return 2;
    }
    if (pid == 0) {
        close(outpipe[0]);
        dup2(outpipe[1], STDOUT_FILENO);
        close(outpipe[1]);
        char *argv[] = { script_path, NULL };
        execve(script_path, argv, environ);
        // Only reached on exec failure. Encode errno so the parent can
        // tell "kernel refused binfmt_script" apart from other exit paths.
        _exit(120 + (errno == EACCES ? 1 : errno == EPERM ? 2 : errno == ENOEXEC ? 3 : 0));
    }

    close(outpipe[1]);
    char out[64] = {0};
    ssize_t n = read(outpipe[0], out, sizeof(out) - 1);
    close(outpipe[0]);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "waitpid: %s", strerror(errno));
        unlink(script_path);
        return 2;
    }
    unlink(script_path);

    if (WIFSIGNALED(status)) {
        fprintf(stderr, "direct exec of shebang script was killed by signal %d", WTERMSIG(status));
        return 1;
    }
    if (!WIFEXITED(status)) {
        fprintf(stderr, "direct exec of shebang script ended abnormally (raw status %d)", status);
        return 1;
    }
    int code = WEXITSTATUS(status);
    if (code >= 120) {
        const char *why = code == 121 ? "EACCES" : code == 122 ? "EPERM" : code == 123 ? "ENOEXEC" : "exec failed";
        fprintf(stderr, "kernel refused to exec shebang script directly: %s", why);
        return 1;
    }
    if (code != 0) {
        fprintf(stderr, "interpreter exited with status %d", code);
        return 1;
    }
    if (n <= 0 || strncmp(out, "shebang-ok", 10) != 0) {
        fprintf(stderr, "script ran but output mismatch: '%s'", out);
        return 1;
    }
    return 0;
}
