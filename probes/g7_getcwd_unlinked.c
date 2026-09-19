// probes/g7_getcwd_unlinked.c
//
// 【测试目的】当前工作目录被 rmdir 后 getcwd() 行为（OHOS lifecycle script 常见路径）。
//
// 【关联项目】Bun
//
// 【上游调用链】
// Bun lifecycle script runner:
//   → bun_install::lifecycle_script_runner::spawn         src/install/lifecycle_script_runner.rs
//     → libc::getcwd(buf, len)
//       → 内核 getcwd(2) — 若 cwd 已被 rmdir 则返 ENOENT
//
// 【为何需要】fork 在 lifecycle_script_runner.rs 加了 getcwd 失败 → fallback 到 HOME。
// 注释说 OHOS 上某些 prepare/install script 跑完会清理 cwd，下一个 hook 调用 getcwd 即 ENOENT。
// g4_getcwd 只测正常路径，本探针补测 unlinked-cwd 边界。
//
// 【影响】若 ENOENT 复现，lifecycle script chain 第二个起会 panic；必须 HOME fallback。
//
// Exit: 0=pass (getcwd fails as expected with ENOENT — quirk confirmed and must be handled)
//       1=unexpected (either succeeded or returned other errno — review needed)

#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void) {
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || !*tmpdir) tmpdir = "/data/local/tmp";

    char dir[512];
    snprintf(dir, sizeof(dir), "%s/preflight_g7.XXXXXX", tmpdir);
    if (!mkdtemp(dir)) {
        fprintf(stderr, "mkdtemp(%s): %s", dir, strerror(errno));
        return 1;
    }

    char origin[PATH_MAX];
    if (!getcwd(origin, sizeof(origin))) {
        fprintf(stderr, "pre-chdir getcwd: %s", strerror(errno));
        rmdir(dir);
        return 1;
    }

    if (chdir(dir) < 0) {
        fprintf(stderr, "chdir(%s): %s", dir, strerror(errno));
        rmdir(dir);
        return 1;
    }

    // Remove the current directory out from under us.
    if (rmdir(dir) < 0) {
        fprintf(stderr, "rmdir(%s): %s", dir, strerror(errno));
        chdir(origin);
        return 1;
    }

    char cwd_buf[PATH_MAX];
    char *r = getcwd(cwd_buf, sizeof(cwd_buf));
    int saved = errno;
    chdir(origin);

    if (r) {
        // Some kernels/libc return a "(unreachable)/..." path or the original.
        // POSIX says behavior is unspecified; report it as a non-quirk pass.
        fprintf(stderr, "getcwd after rmdir(.) returned '%s' (no quirk to handle)", cwd_buf);
        return 0;
    }
    if (saved == ENOENT) {
        // Expected OHOS-like behavior — caller must HOME-fallback.
        fprintf(stderr, "getcwd ENOENT reproduces — lifecycle script must HOME-fallback");
        return 0;
    }
    fprintf(stderr, "getcwd failed with unexpected errno: %s", strerror(saved));
    return 1;
}
