// probes/i1_chmod_hmdfs.c
//
// 【测试目的】chmod() 在 HMDFS 文件系统上是否生效。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

static int test_chmod(const char *dir) {
    char path[512];
    snprintf(path, sizeof(path), "%s/chmod_probe_%d.XXXXXX", dir, getpid());

    int fd = mkstemp(path);
    if (fd < 0) {
        fprintf(stderr, "mkstemp failed in %s: %s", dir, strerror(errno));
        return -1;
    }
    close(fd);

    // Test: chmod 600 (owner rw, group 0, other 0)
    if (chmod(path, 0600) != 0) {
        fprintf(stderr, "chmod(%s, 0600) failed: %s", path, strerror(errno));
        unlink(path);
        return -1;
    }

    struct stat st;
    if (stat(path, &st) != 0) {
        unlink(path);
        return -1;
    }

    mode_t mode = st.st_mode & 0777;
    unlink(path);

    // On HMDFS: group forced to 6, other to 0. So 0600 → 0660.
    // Standard: 0600 → 0600
    if (mode != 0600) {
        fprintf(stderr, "chmod 0600 produced 0%03o (expected 0600): HMDFS-like override detected", mode);
        return 1;
    }
    return 0;
}

int main(void) {
    // 注意：不能用 NULL terminator 终止 — getenv("TMPDIR") 在容器中
    // 返回 NULL，会让 `for (; dirs[i]; ...)` 在 i=0 立刻退出，
    // 跳过后面的 /tmp 和 . 候选。改用固定长度遍历。
    const char *dirs[] = {
        getenv("TMPDIR"),
        getenv("HOME"),
        "/tmp",
        ".",
    };
    const int N = (int)(sizeof(dirs) / sizeof(dirs[0]));

    for (int i = 0; i < N; i++) {
        if (!dirs[i] || !*dirs[i]) continue;
        if (access(dirs[i], W_OK) != 0) continue;
        int r = test_chmod(dirs[i]);
        if (r >= 0) return r;
    }

    fprintf(stderr, "no writable directory found for chmod test");
    return 2;
}

