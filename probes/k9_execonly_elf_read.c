// probes/k9_execonly_elf_read.c
//
// 【测试目的】验证 chmod 0111（只有执行位、没有读位）的 ELF 能否通过
// open("/proc/self/exe") 读取自身。
//
// 【关联项目】ohos-bun（bun build --compile 单体可执行文件）
//
// 【上游调用链】
// 单体可执行文件启动时读取自带的 .bun 载荷段:
//   → open("/proc/self/exe", O_RDONLY) 读取 ELF section header 定位载荷
//     → Err(_) 分支: "OHOS hmdfs denies open(/proc/self/exe) for files
//       lacking the read bit"                    StandaloneModuleGraph.rs:588
//       → 改读 /proc/self/maps 找 PIE 装载基址，靠链接期 vaddr 直接算出
//         运行时地址，绕开对文件本身的读操作
//
// 【为何需要】这条兜底逻辑相当复杂（解析 /proc/self/maps、手工做 ASLR
// 偏移计算），如果 hmdfs 已经不再拒绝这类 open，可以整段删除。
//
// 复用 k1 的"以特殊 argv 重新进入自身、扮演子进程角色"手法：把自身拷贝
// 一份、chmod 0111（无读位）、exec 它，让那份拷贝在运行时尝试读自己的
// /proc/self/exe。
//
// Exit: 0=pass (0111 的 ELF 仍能读到自己的 /proc/self/exe -- hmdfs 的
//              这条限制已经解除，StandaloneModuleGraph.rs 的 maps 兜底
//              可以删除)
//       1=fail (读失败 EACCES -- 兜底仍然必需)
//       2=unsupported (自举步骤本身失败，测不出这个更具体的问题)

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

#define CHILD_FLAG "--k9-child"

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
        // Running as the execute-only copy. The real test: can *this*
        // process read its own backing file via /proc/self/exe despite
        // having no read permission bit set on it?
        int fd = open("/proc/self/exe", O_RDONLY);
        if (fd < 0) {
            return (errno == EACCES) ? 1 : 2;
        }
        char one_byte;
        ssize_t n = read(fd, &one_byte, 1);
        close(fd);
        return (n >= 0) ? 0 : 1;
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
    snprintf(copy_path, sizeof(copy_path), "%s/.ohos-preflight-k9-%d", tmpdir, getpid());

    if (copy_file(self_path, copy_path) != 0) {
        fprintf(stderr, "failed to stage a copy at %s: %s", copy_path, strerror(errno));
        return 2;
    }
    // Execute-only: no read bit for owner/group/other.
    if (chmod(copy_path, 0111) != 0) {
        fprintf(stderr, "chmod(0111): %s", strerror(errno));
        unlink(copy_path);
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
        _exit(125); // couldn't even exec the execute-only copy itself
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "waitpid: %s", strerror(errno));
        unlink(copy_path);
        return 2;
    }
    unlink(copy_path);

    if (!WIFEXITED(status)) {
        fprintf(stderr, "execute-only copy ended abnormally (raw status %d)", status);
        return 2;
    }
    int code = WEXITSTATUS(status);
    if (code == 125) {
        fprintf(stderr, "couldn't even exec the execute-only (0111) copy -- can't test this scenario");
        return 2;
    }
    if (code == 1) {
        fprintf(stderr, "open(/proc/self/exe) on an execute-only (0111) binary failed with EACCES -- StandaloneModuleGraph.rs maps-based fallback still required");
        return 1;
    }
    if (code != 0) {
        fprintf(stderr, "child-mode exited with unexpected status %d", code);
        return 2;
    }
    return 0;
}
