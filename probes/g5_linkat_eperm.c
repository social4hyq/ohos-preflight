// probes/g5_linkat_eperm.c
//
// 【测试目的】在受限目录下 linkat() 是否返 EPERM（OHOS 沙箱常见 quirk）。
//
// 【关联项目】Bun
//
// 【上游调用链】
// Bun isolated install / cache hardlink:
//   → bun_install::Hardlinker::link(src, dst)             src/install/isolated_install/Hardlinker.rs
//     → libc::linkat(AT_FDCWD, src, AT_FDCWD, dst, 0)
//       → 内核 linkat(2)
//
// 【为何需要】fork 在 src/install/PackageManager.rs / Hardlinker.rs 加了
// hardlink 失败回退到 copy 的兜底，注释说 OHOS 部分目录 linkat 返 EPERM。
// 本探针验证 quirk 是否复现。
//
// 【影响】若 linkat EPERM 复现，bun install 在 OHOS 上必须走 copy fallback；
// 否则 isolated install 全链断裂（依赖 hardlink 去重）。
//
// Exit: 0=pass (linkat succeeds), 1=fail (EPERM/EACCES quirk reproduces)

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void) {
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || !*tmpdir) tmpdir = "/data/local/tmp";

    char src[512], dst[512];
    snprintf(src, sizeof(src), "%s/preflight_g5_src.XXXXXX", tmpdir);
    int fd = mkstemp(src);
    if (fd < 0) {
        fprintf(stderr, "mkstemp(%s): %s", src, strerror(errno));
        return 1;
    }
    if (write(fd, "x", 1) != 1) {
        fprintf(stderr, "write: %s", strerror(errno));
        close(fd); unlink(src); return 1;
    }
    close(fd);

    snprintf(dst, sizeof(dst), "%s.link", src);
    unlink(dst);

    int rc = 0;
    if (linkat(AT_FDCWD, src, AT_FDCWD, dst, 0) < 0) {
        const char *note = "";
        if (errno == EPERM) note = " — OHOS sandbox quirk reproduces, hardlink→copy fallback required";
        else if (errno == EACCES) note = " — EACCES variant, same class of restriction";
        fprintf(stderr, "linkat(%s -> %s): %s%s", src, dst, strerror(errno), note);
        rc = 1;
    } else {
        struct stat st;
        if (stat(dst, &st) < 0 || st.st_nlink < 2) {
            fprintf(stderr, "post-linkat stat nlink=%lu (expected >= 2)",
                    (unsigned long)(st.st_nlink));
            rc = 1;
        }
        unlink(dst);
    }
    unlink(src);
    return rc;
}
