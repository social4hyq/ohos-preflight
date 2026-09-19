// solutions/i4_app_private_dir.c
//
// 替代 /data/local/tmp：使用应用私有数据目录。
// OHOS 沙箱限制访问 /data/local/tmp，应使用应用 sandbox 内的 data 目录。
//
// 验证：在 $HOME 下创建文件，测试读写。

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

int main(void) {
    const char *dir = getenv("HOME");
    if (!dir) dir = "/tmp";  // 容器回退

    char path[256];
    snprintf(path, sizeof(path), "%s/ohos-sol-test-%d.tmp", dir, getpid());

    FILE *f = fopen(path, "w+");
    if (!f) {
        fprintf(stderr, "open failed: %s", strerror(errno));
        return 1;
    }
    if (fputs("ok", f) < 0) {
        fprintf(stderr, "write failed: %s", strerror(errno));
        fclose(f); unlink(path);
        return 1;
    }
    fclose(f);
    unlink(path);
    return 0;
}
