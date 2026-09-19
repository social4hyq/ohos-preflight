// 替代：用 OHOS 应用私有目录（HAP context.cacheDir / filesDir）替代 /data/local/tmp
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int write_app_tmp(const char *payload, size_t n) {
    const char *dir = getenv("HOME");
    if (!dir) dir = "/data/storage/el2/base/temp";  // OHOS 应用沙箱 temp 区
    char path[256];
    snprintf(path, sizeof path, "%s/data-%d.tmp", dir, getpid());
    FILE *f = fopen(path, "w+");
    if (!f) return -1;
    fwrite(payload, 1, n, f);
    fclose(f);
    unlink(path);
    return 0;
}
// TODO: 待 HarmonyOS 放行 /data/local/tmp 访问权限后可恢复 ADB push 工作流
