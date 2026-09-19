// probes/j12_fchmodat2_fb.c
//
// 【测试目的】c5_fchmodat2 替代：fchmodat() 降级（无 flags 支持）。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void) {
    const char *dir = getenv("HOME");
    if (!dir || !*dir) dir = "/data/storage/el2/base";
    char path[256];
    snprintf(path, sizeof path, "%s/j12_mode_%d", dir, getpid());

    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if (fd < 0) { fprintf(stderr, "open(%s): %m", path); return 1; }
    close(fd);

    if (fchmodat(AT_FDCWD, path, 0644, 0) < 0) {
        fprintf(stderr, "fchmodat(%s, 0644): %m", path);
        unlink(path);
        return 1;
    }

    struct stat st;
    if (stat(path, &st) < 0) {
        fprintf(stderr, "stat: %m");
        unlink(path);
        return 1;
    }
    unlink(path);

    mode_t got = st.st_mode & 0777;
    if (got != 0600) return 0;
    fprintf(stderr, "fchmodat did not change mode (still 0%o)", got);
    return 1;
}

