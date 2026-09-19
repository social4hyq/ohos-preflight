// probes/g9_bun_cache_linkat.c
//
// 【测试目的】在 ~/.bun/install/cache 风格的真实路径下 linkat() 是否成功
//             — 补 g5 只测了 TMPDIR (/data/storage/el3/base) 的盲点。
//
// 【关联项目】Bun
//
// 【上游调用链】
// Bun install isolated 模式从 cache 硬链到 store/node_modules:
//   → bun_install::Hardlinker::link(cache_src, store_dst)     src/install/isolated_install/Hardlinker.rs
//     → libc::linkat(AT_FDCWD, "$HOME/.bun/install/cache/...", AT_FDCWD, "...", 0)
//
// 【为何需要】g5 在 /data/storage/el3/base 下两端结果（HM EACCES / OH PASS），
// 但 Bun 实际跑在 $HOME/.bun/install/cache，文件系统/权限链不同。
// 验证真实安装路径下是否仍 EACCES。
//
// 【影响】若 EACCES：Hardlinker copy fallback 必需；若 PASS：fork 兜底是 over-protection。
//
// Exit: 0=pass (linkat 成功), 1=fail (EACCES/EPERM)

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int try_mkdir_p(const char *path) {
    char tmp[512];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

int main(void) {
    const char *home = getenv("HOME");
    if (!home || !*home) home = "/data/storage/el2/base";

    char cache_dir[512], store_dir[512], src[600], dst[600];
    snprintf(cache_dir, sizeof(cache_dir), "%s/.bun/install/cache", home);
    snprintf(store_dir, sizeof(store_dir), "%s/.bun/install/store", home);

    if (try_mkdir_p(cache_dir) < 0) {
        fprintf(stderr, "mkdir(%s): %s", cache_dir, strerror(errno));
        return 1;
    }
    if (try_mkdir_p(store_dir) < 0) {
        fprintf(stderr, "mkdir(%s): %s", store_dir, strerror(errno));
        return 1;
    }

    snprintf(src, sizeof(src), "%s/preflight_g9.src.XXXXXX", cache_dir);
    int fd = mkstemp(src);
    if (fd < 0) {
        fprintf(stderr, "mkstemp(%s): %s", src, strerror(errno));
        return 1;
    }
    write(fd, "x", 1);
    close(fd);

    snprintf(dst, sizeof(dst), "%s/preflight_g9.dst", store_dir);
    unlink(dst);

    int rc = 0;
    if (linkat(AT_FDCWD, src, AT_FDCWD, dst, 0) < 0) {
        const char *note = "";
        if (errno == EACCES || errno == EPERM) {
            note = " — Hardlinker copy fallback required at real bun cache→store path";
        } else if (errno == EXDEV) {
            note = " — cross-device link (different fs), copy fallback required (expected on some sandboxes)";
        }
        fprintf(stderr, "linkat(%s -> %s): %s%s", src, dst, strerror(errno), note);
        rc = 1;
    } else {
        unlink(dst);
    }
    unlink(src);
    return rc;
}
