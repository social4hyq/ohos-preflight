// probes/g4_getcwd.c
//
// 【测试目的】getcwd() 获取当前工作目录 — Bun CLI/resolver/fs/shell 等 30+ 调用点。
//
// 【关联项目】Bun
//
// 【上游调用链】
// Bun CLI 入口 / 模块解析 / Node.js process.cwd() / fs 相对路径
//   → bun_core::getcwd(buf)                                   src/bun_core/util.rs:4350
//     → libc::getcwd(buf.as_mut_ptr().cast(), buf.len())      同文件:4356
//       → 内核 getcwd(2)
// 
// Bun 事件循环 / Shell / Glob / lockfile / bundler / fetch 等
//   → bun_sys::getcwd(&mut buf[..])                            src/sys/lib.rs:2648
//   → bun_sys::getcwd_alloc() / getcwd_z()                     src/sys/lib.rs:904-914
//     → libc::getcwd(...)
//       → 内核 getcwd(2)
// 
// 所有调用点（Rust 侧）:
//   bun_core::getcwd           — 核心封装
//   bun_sys::Syscall::getcwd   — syscall 层
//   crash_handler               — 崩溃报告
//   MiniEventLoop               — 事件循环启动
//   GlobWalker                  — glob 基路径
//   lockfile / PackageManager   — 包管理 cwd
//   patchPackage / WorkspaceMap — workspace 路径
//   resolver::fs / resolver     — 模块解析
//   run_command / Arguments     — bun run 入口
//   audit_command / exec_command
//   install_completions_command
//   pack_command / package_manager_command
//   node_fs / node_process      — Node.js 兼容 (process.cwd, fs)
//   shell/interpreter / rm      — Shell 解释器
//   fetch / bake / bundler      — Web API / 构建 / 打包
//   StandaloneModuleGraph       — standalone bundle
//   ansi_renderer               — markdown 渲染
//
// 【影响】Bun 在 CLI 入口、模块解析、Node.js process.cwd()、fs 相对路径、shell 解释器、包管理、lockfile、bundler、事件循环启动等 30+ 处调用 getcwd()。若 getcwd() 失败或返回不可访问的路径，Bun 将无法启动或功能异常（如 ENOENT → bun run 直接 panic）。
//
#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define QUIET_FAIL(step, fn, e) do { \
    fprintf(stderr, "[%s] %s: %s", step, fn, strerror(e)); \
    return 1; \
} while (0)

int main(void) {
    char cwd_buf[PATH_MAX];
    struct stat st;

    // ── 1. Pre-allocated buffer (how Bun actually calls it) ────────────
    //    bun_core::getcwd / Syscall::getcwd both pass a fixed-size buffer
    //    and use strlen() to measure the NUL-terminated result.
    if (!getcwd(cwd_buf, sizeof(cwd_buf))) {
        QUIET_FAIL("1-buf", "getcwd(buf, PATH_MAX)", errno);
    }

    // Verify strlen works on the returned buffer (Bun calls libc::strlen).
    size_t len = strlen(cwd_buf);
    if (len == 0 || len >= PATH_MAX) {
        fprintf(stderr, "[1-buf] getcwd returned empty or truncated path (len=%zu)", len);
        return 1;
    }

    // Verify the path is a real accessible directory.
    if (stat(cwd_buf, &st) != 0) {
        fprintf(stderr, "[1-buf] stat('%s') failed: %s", cwd_buf, strerror(errno));
        return 1;
    }
    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "[1-buf] '%s' is not a directory", cwd_buf);
        return 1;
    }

    // ── 2. Small buffer (Bun uses MAX_PATH_BYTES=4096, but a too-small  ──
    //    buffer should produce ERANGE, not crash or return garbage).
    char small[2];
    if (getcwd(small, sizeof(small))) {
        fprintf(stderr, "[2-small] getcwd(buf, 2) unexpectedly succeeded with '%s'", small);
        return 1;
    }
    if (errno != ERANGE) {
        fprintf(stderr, "[2-small] getcwd(buf, 2) expected ERANGE, got: %s", strerror(errno));
        return 1;
    }

    // ── 3. /proc/self/cwd symlink (musl fast path) ─────────────────────
    //    musl's getcwd() tries readlink("/proc/self/cwd") first. If this
    //    symlink is missing or EACCES, musl falls back to manual traversal,
    //    which is slower and requires read permission on every parent dir.
    char proc_cwd[PATH_MAX];
    ssize_t n = readlink("/proc/self/cwd", proc_cwd, sizeof(proc_cwd) - 1);
    if (n < 0) {
        // musl will use the manual parent-traversal fallback — verify that
        // the manual path also works (step 1 already proved getcwd works,
        // but we note that the fast path is unavailable).
        fprintf(stderr, "[3-proc] readlink(/proc/self/cwd): %s (musl will use manual traversal)",
                strerror(errno));
        // NOT a failure — just means musl falls back to the slow path.
    } else {
        proc_cwd[n] = '\0';
        // The symlink should point to the same directory getcwd returned.
        if (strcmp(cwd_buf, proc_cwd) != 0) {
            fprintf(stderr, "[3-proc] /proc/self/cwd -> '%s' differs from getcwd() -> '%s'",
                    proc_cwd, cwd_buf);
            return 1;
        }
    }

    // ── 4. POSIX allocate-on-NULL (glibc/musl extension, not used by ────
    //    Bun directly, but verifies libc can internally allocate).
    char *allocated = getcwd(NULL, 0);
    if (!allocated) {
        fprintf(stderr, "[4-alloc] getcwd(NULL, 0) failed: %s", strerror(errno));
        return 1;
    }
    if (strcmp(cwd_buf, allocated) != 0) {
        fprintf(stderr, "[4-alloc] getcwd(NULL,0)='%s' differs from getcwd(buf)='%s'",
                allocated, cwd_buf);
        free(allocated);
        return 1;
    }
    free(allocated);

    // ── 5. Path component traversal (musl fallback needs this) ─────────
    //    musl's manual getcwd walks up the directory tree: opens "..",
    //    reads entries until it finds the current inode, repeats until
    //    root. This requires read+exec permission on every ancestor.
    //    Verify we can stat each ancestor component.
    char path_copy[PATH_MAX];
    strcpy(path_copy, cwd_buf);
    for (char *p = path_copy; *p; p++) {
        if (*p == '/' && p != path_copy) {
            *p = '\0';
            if (stat(path_copy, &st) != 0) {
                fprintf(stderr, "[5-ancestor] stat('%s') failed: %s",
                        path_copy, strerror(errno));
                return 1;
            }
            *p = '/';
        }
    }

    return 0;
}

