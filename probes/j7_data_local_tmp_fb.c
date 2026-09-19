// probes/j7_data_local_tmp_fb.c
//
// 【测试目的】i4_data_local_tmp 替代：fopen() 落到 $HOME / 沙箱 temp 目录。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    const char *dir = getenv("HOME");
    if (!dir || !*dir) dir = "/data/storage/el2/base/temp";
    char path[256];
    snprintf(path, sizeof path, "%s/j7_app_%d.tmp", dir, getpid());

    FILE *f = fopen(path, "w+");
    if (!f) {
        fprintf(stderr, "fopen(%s): %m", path);
        return 1;
    }
    size_t n = fwrite("ok", 1, 2, f);
    fclose(f);
    unlink(path);
    if (n != 2) {
        fprintf(stderr, "write returned %zu (expected 2)", n);
        return 1;
    }
    return 0;
}

