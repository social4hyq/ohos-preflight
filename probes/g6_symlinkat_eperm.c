// probes/g6_symlinkat_eperm.c
//
// 【测试目的】symlinkat() 在受限目录是否返 EPERM。
//
// 【关联项目】Bun
//
// 【上游调用链】
// Bun PackageInstall bin 链接:
//   → bun_install::PackageInstall::link_bin                src/install/PackageInstall.rs
//     → libc::symlinkat(target, AT_FDCWD, linkpath)
//       → 内核 symlinkat(2)
//
// 【为何需要】fork 在 PackageInstall.rs 加了 symlinkat EPERM 兜底。
// OHOS 沙箱在部分私有目录下拒绝 symlinkat。
//
// 【影响】若复现，bun install 创建 node_modules/.bin/* 必须 fallback 到拷贝可执行脚本。
//
// Exit: 0=pass, 1=fail (EPERM/EACCES quirk reproduces)

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

    char target[512], linkpath[512];
    snprintf(target, sizeof(target), "%s/preflight_g6_target.XXXXXX", tmpdir);
    int fd = mkstemp(target);
    if (fd < 0) {
        fprintf(stderr, "mkstemp(%s): %s", target, strerror(errno));
        return 1;
    }
    close(fd);

    snprintf(linkpath, sizeof(linkpath), "%s.symlink", target);
    unlink(linkpath);

    int rc = 0;
    if (symlinkat(target, AT_FDCWD, linkpath) < 0) {
        const char *note = "";
        if (errno == EPERM) note = " — OHOS sandbox quirk reproduces, bin-symlink→copy fallback required";
        else if (errno == EACCES) note = " — EACCES variant, same class of restriction";
        fprintf(stderr, "symlinkat(%s -> %s): %s%s", target, linkpath, strerror(errno), note);
        rc = 1;
    } else {
        struct stat st;
        if (lstat(linkpath, &st) < 0 || !S_ISLNK(st.st_mode)) {
            fprintf(stderr, "post-symlinkat lstat: not a symlink (mode=%o)", st.st_mode);
            rc = 1;
        }
        unlink(linkpath);
    }
    unlink(target);
    return rc;
}
