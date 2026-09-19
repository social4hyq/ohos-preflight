// solutions/g1_tmpfile_mkstemp.c
//
// 替代 tmpfile()：在应用私有目录用 mkstemp() 创建临时文件。
// tmpfile() 依赖 P_tmpdir（在 OHOS 沙箱中不可用），
// 改用 mkstemp() + 应用 data 目录即可绕过。
//
// 验证方式：本机和容器两端编译运行，exit 0 = pass。

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int main(void) {
    // OHOS 应用可用 getenv("HOME") 或硬编码相对路径
    const char *dir = getenv("TMPDIR");
    if (!dir) dir = getenv("HOME");
    if (!dir) dir = "/tmp";  // 容器回退

    char tmpl[256];
    snprintf(tmpl, sizeof(tmpl), "%s/ohos-preflight-sol-XXXXXX", dir);

    int fd = mkstemp(tmpl);
    if (fd < 0) {
        fprintf(stderr, "mkstemp failed: %s", strerror(errno));
        return 1;
    }
    // 写入验证
    if (write(fd, "ok", 2) != 2) {
        fprintf(stderr, "write failed: %s", strerror(errno));
        unlink(tmpl); close(fd);
        return 1;
    }
    unlink(tmpl);
    close(fd);
    return 0;
}
