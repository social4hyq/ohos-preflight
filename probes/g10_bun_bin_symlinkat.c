// probes/g10_bun_bin_symlinkat.c
//
// 【测试目的】在 node_modules/.bin 风格目录下 symlinkat() 是否成功
//             — 补 g6 只测了 TMPDIR (/data/storage/el3/base) 的盲点。
//
// 【关联项目】Bun
//
// 【上游调用链】
// Bun install 创建可执行链接:
//   → bun_install::PackageInstall::link_bin                  src/install/PackageInstall.rs
//     → libc::symlinkat("../package/bin.js", AT_FDCWD, "node_modules/.bin/foo")
//
// 【为何需要】g6 在 /data/storage/el3/base 下两端 PASS，但 Bun 实际跑在 cwd 下的
// node_modules/.bin，可能在某些 mount/sandbox 路径下 EPERM。
// 模拟真实路径布局（在 cwd 下创建 node_modules/.bin）。
//
// 【影响】若 EPERM：PackageInstall fallback to copy 必需；若 PASS：fork 兜底多余。
//
// Exit: 0=pass (symlinkat 成功), 1=fail

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void) {
    char tmpl[] = "preflight_g10_root.XXXXXX";
    if (!mkdtemp(tmpl)) {
        fprintf(stderr, "mkdtemp(%s): %s", tmpl, strerror(errno));
        return 1;
    }

    char nm_bin[256], target[256], linkpath[512];
    snprintf(nm_bin, sizeof(nm_bin), "%s/node_modules/.bin", tmpl);
    if (mkdir(nm_bin, 0755) != 0) {
        // also need parent
        char nm[256];
        snprintf(nm, sizeof(nm), "%s/node_modules", tmpl);
        mkdir(nm, 0755);
        if (mkdir(nm_bin, 0755) != 0) {
            fprintf(stderr, "mkdir(%s): %s", nm_bin, strerror(errno));
            rmdir(tmpl);
            return 1;
        }
    }

    snprintf(target, sizeof(target), "%s/node_modules/foo/bin.js", tmpl);
    // create a fake target file (don't need it to exist for symlinkat itself,
    // but create it so post-checks work)
    char foo_dir[256];
    snprintf(foo_dir, sizeof(foo_dir), "%s/node_modules/foo", tmpl);
    mkdir(foo_dir, 0755);
    int tf = open(target, O_CREAT | O_WRONLY, 0644);
    if (tf >= 0) close(tf);

    snprintf(linkpath, sizeof(linkpath), "%s/foo", nm_bin);

    int rc = 0;
    if (symlinkat("../foo/bin.js", AT_FDCWD, linkpath) < 0) {
        const char *note = (errno == EPERM || errno == EACCES)
            ? " — PackageInstall bin-symlink → copy fallback required at real node_modules/.bin path"
            : "";
        fprintf(stderr, "symlinkat(../foo/bin.js -> %s): %s%s", linkpath, strerror(errno), note);
        rc = 1;
    } else {
        unlink(linkpath);
    }

    unlink(target);
    rmdir(foo_dir);
    rmdir(nm_bin);
    char nm[256];
    snprintf(nm, sizeof(nm), "%s/node_modules", tmpl);
    rmdir(nm);
    rmdir(tmpl);
    return rc;
}
