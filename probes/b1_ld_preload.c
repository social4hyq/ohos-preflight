// probes/b1_ld_preload.c
//
// 【测试目的】LD_PRELOAD 动态库预加载是否生效。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (getenv("B1_PRELOAD_PASS") == NULL) {
        // First pass: re-exec ourselves with LD_PRELOAD + sentinel guard env.
        setenv("LD_PRELOAD", "./probes/libb1hook.so", 1);
        setenv("B1_PRELOAD_PASS", "1", 1);
        execv(argv[0], argv);
        perror("execv");
        return 2;
    }

    const char *base = getenv("HOME");
    if (!base) base = "/tmp";
    char path[320];
    snprintf(path, sizeof(path), "%s/.tmp/b1_hook.%d", base, (int)getpid());
    struct stat st;
    if (stat(path, &st) == 0) {
        unlink(path);
        return 0;
    }
    fprintf(stderr, "LD_PRELOAD did not load libb1hook.so (no %s)\n", path);
    return 1;
}

