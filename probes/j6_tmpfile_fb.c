// probes/j6_tmpfile_fb.c
//
// 【测试目的】g1_tmpfile 替代：mkstemp() 落到 $HOME / 沙箱基目录。
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
    if (!dir || !*dir) dir = "/data/storage/el2/base";
    char tmpl[256];
    snprintf(tmpl, sizeof tmpl, "%s/j6_tmpXXXXXX", dir);

    int fd = mkstemp(tmpl);
    if (fd < 0) {
        fprintf(stderr, "mkstemp(%s): %m", tmpl);
        return 1;
    }
    FILE *fp = fdopen(fd, "w+");
    if (!fp) {
        fprintf(stderr, "fdopen failed");
        close(fd); unlink(tmpl);
        return 1;
    }
    int rc = (fputs("ok", fp) != EOF) ? 0 : 1;
    fclose(fp);
    unlink(tmpl);
    if (rc) fprintf(stderr, "write to mkstemp file failed");
    return rc;
}

