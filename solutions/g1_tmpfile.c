// 替代：mkstemp() + 应用私有目录（OHOS HAP 通过 getenv("HOME") 暴露）
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

FILE *tmpfile_in_home(void) {
    const char *dir = getenv("HOME");
    if (!dir) dir = "/data/storage/el2/base";  // OHOS 应用沙箱 base
    char tmpl[256];
    snprintf(tmpl, sizeof tmpl, "%s/tmp-XXXXXX", dir);
    int fd = mkstemp(tmpl);
    if (fd < 0) return NULL;
    unlink(tmpl);  // 进程退出自动清理
    return fdopen(fd, "w+");
}
// TODO: 待 HarmonyOS 放行 /data/local/tmp 或修复 OHOS musl tmpfile() 路径后改回 tmpfile()
