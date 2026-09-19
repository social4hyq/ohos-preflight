# OpenHarmony vs HarmonyOS 系统能力对比报告
> 生成时间：2026-09-19 19:06

## 概览

| 指标 | 值 |
|------|-----|
| 探针总数 | 75 |
| 两端一致通过 | 53 (70.7%) |
| 需申请放行 | 15 (20.0%) |
| 两端共同失败 | 1 (1.3%) |
| 异常 | 3 |
| 无法测定（环境问题） | 3 |

## 🔴 需申请放行 — OH 通过但 HM 受限

### P0 · 阻断运行（进程崩溃或无法启动）（1 项）

- **`c13_epoll_pwait2`** **[Bun]** — epoll_pwait2 纳秒级 epoll — Bun platform/linux.rs:39
  - **影响：** Bun 事件循环使用 epoll_pwait2 实现纳秒级超时精度。调用被 seccomp 拦截（SIGSYS），Bun 无法启动事件循环，影响所有异步 I/O 操作。
  - **触发路径：** `Bun 事件循环纳秒级超时等待
  → sys_epoll_pwait2(epfd, events, max,                  bun/src/platform/linux.rs:29
      timeout, sigmask)
    → libc::syscall(SYS_epoll_pwait2, epfd, events,      libc crate
        max, timeout, sigmask, 8)
      → seccomp filter → SIGSYS`
  - **建议：** 向 HarmonyOS 团队申请放行 epoll_pwait2 (441)；短期用 epoll_pwait 降级（毫秒精度）
  - **替代实现**（HarmonyOS 支持前的过渡方案） — ✔ 双轨验证通过（`j5_epoll_pwait2_fb`）：

    ```c
    // 替代：epoll_pwait（毫秒精度）+ timerfd 补齐纳秒级唤醒
    // timerfd 注册到同一个 epoll，到期触发 epoll 立即返回
    #include <sys/epoll.h>
    #include <sys/timerfd.h>
    #include <time.h>
    #include <unistd.h>
    
    int epoll_pwait2_fallback(int epfd, struct epoll_event *evs, int max,
                              const struct timespec *ts,
                              const sigset_t *sigmask) {
        if (!ts) return epoll_pwait(epfd, evs, max, -1, sigmask);
    
        // 纳秒精度场景：用 timerfd 替代 timeout 参数
        int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
        struct itimerspec spec = { .it_value = *ts };
        timerfd_settime(tfd, 0, &spec, NULL);
        struct epoll_event tev = { .events = EPOLLIN, .data.fd = tfd };
        epoll_ctl(epfd, EPOLL_CTL_ADD, tfd, &tev);
    
        int n = epoll_pwait(epfd, evs, max, -1, sigmask);
        epoll_ctl(epfd, EPOLL_CTL_DEL, tfd, NULL);
        close(tfd);
    
        // 过滤掉 timerfd 自身的事件
        int out = 0;
        for (int i = 0; i < n; i++)
            if (evs[i].data.fd != tfd) evs[out++] = evs[i];
        return out;
    }
    // TODO: 待 HarmonyOS 放行 epoll_pwait2 (441) 后改用
    //   syscall(SYS_epoll_pwait2, epfd, evs, max, ts, sigmask, sizeof(sigset_t))
    ```

### P1 · 影响核心功能（8 项）

- **`a1_seccomp_unotify`** — seccomp 用户态通知 (SECCOMP_IOCTL_NOTIF_RECV)
  - **影响：** seccomp 用户态通知机制用于实现用户空间 syscall 拦截（调试器、沙箱、profiler）。HarmonyOS 拒绝 SECCOMP_SET_MODE_FILTER，调试工具链（如 strace 替代品、Node inspector）无法工作。
  - **触发路径：** `调试工具 (strace / seccomp sandbox)
  → seccomp(SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_NEW_LISTENER)
    → 返回 EINVAL（参数无效 / 沙箱禁止）

vite-plus fspy 系统调用拦截:
  → vite-task/crates/fspy_seccomp_unotify — seccomp 用户态通知
  → seccomp(SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_NEW_LISTENER)
  → seccompiler (rust-vmm/seccompiler) — BPF filter 编译`
  - **建议：** 向 HarmonyOS 团队申请放行 seccomp 能力；libc 层调用审计可用 LD_PRELOAD shim 替代（拦截 libc 函数 + RTLD_NEXT 转发，无法拦截 raw syscall）
  - **替代实现**（HarmonyOS 支持前的过渡方案） — ✔ 双轨验证通过（`j13_seccomp_shim_fb`）：

    ```c
    // 替代：用 LD_PRELOAD shim 拦截 libc 函数（无法拦截 raw syscall）
    // 适用场景：监控/审计应用对 libc 文件、网络、进程接口的调用
    #define _GNU_SOURCE
    #include <dlfcn.h>
    #include <fcntl.h>
    #include <stdio.h>
    
    typedef int (*orig_open_t)(const char *, int, ...);
    int open(const char *path, int flags, ...) {
        static orig_open_t real = NULL;
        if (!real) real = (orig_open_t)dlsym(RTLD_NEXT, "open");
        fprintf(stderr, "[shim] open(%s, %d)\n", path, flags);
        // 自定义策略：拒绝 / 重写路径 / 记录审计日志
        return real(path, flags);
    }
    // 编译：clang -shared -fPIC shim.c -o libshim.so -ldl
    // 运行：LD_PRELOAD=./libshim.so ./target_app
    // 局限：无法拦截直接 syscall(...)；如需 raw syscall 拦截只能等放行
    // TODO: 待 HarmonyOS 放行 seccomp(SECCOMP_SET_MODE_FILTER) 后改用标准 seccomp-unotify 方案
    ```
- **`a10_close_range`** **[Bun]** — close_range (436) 批量关闭 fd — Bun add fd 清理
  - **影响：** Bun 在 spawn 子进程后调用 close_range 批量关闭继承的文件描述符。调用被 seccomp 拦截（SIGSYS），子进程 fd 泄漏或异常退出，影响 Bun 的进程管理功能。
  - **触发路径：** `Bun.spawn() 子进程 fd 清理
  → POSIX_SPAWN_CLOEXEC_DEFAULT (0x4000)               bun/src/spawn_sys/spawn_process.rs:578
  → fcntl(F_SETFD, FD_CLOEXEC) per fd                   bun/src/spawn_sys/spawn_process.rs:612
    → libc::syscall(SYS_close_range=436)                内核 close_range(2)
      → seccomp filter → SIGSYS`
  - **建议：** 向 HarmonyOS 团队申请放行 close_range (436)；短期可在子进程中逐个 close fd 代替
  - **替代实现**（HarmonyOS 支持前的过渡方案） — ✔ 双轨验证通过（`j2_close_range_fb`）：

    ```c
    // 替代：通过 /proc/self/fd 枚举实际打开的 fd，逐个 close
    // 比线性 for 循环到 RLIMIT_NOFILE (常为 65536) 快 100~1000 倍
    #include <dirent.h>
    #include <stdlib.h>
    #include <unistd.h>
    
    int close_range_fallback(unsigned int low, unsigned int high,
                             unsigned int flags /* CLOSE_RANGE_CLOEXEC ... */) {
        DIR *d = opendir("/proc/self/fd");
        int dirfd_ = d ? dirfd(d) : -1;
        if (!d) {
            // 极端兜底：线性扫描
            for (unsigned int fd = low; fd <= high && fd < 65536; fd++) close(fd);
            return 0;
        }
        struct dirent *e;
        while ((e = readdir(d))) {
            if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
            unsigned int fd = (unsigned int)atoi(e->d_name);
            if (fd >= low && fd <= high && (int)fd != dirfd_) close(fd);
        }
        closedir(d);
        return 0;
    }
    // TODO: 待 HarmonyOS 放行 close_range (436) 后改用 syscall(SYS_close_range, lo, hi, flags)
    ```
- **`c4_openat2`** **[Bun]** — openat2 (437) 扩展文件打开 — Bun 受限文件
  - **影响：** Bun 使用 openat2 打开文件时指定 RESOLVE_* 标志以防止路径逃逸。调用被 seccomp 拦截（SIGSYS），影响 Bun 的受限文件打开路径，回退到 openat() 时缺少安全约束。
  - **触发路径：** `Bun 文件打开（防路径逃逸）
  → linux_syscall::openat2_beneath(dir, path, flags, mode)   bun/src/sys/linux_syscall.rs:93
    → rustix::fs::openat2(dir, path, oflags, mode,
        ResolveFlags::BENEATH)
      → seccomp filter → SIGSYS`
  - **建议：** 向 HarmonyOS 团队申请放行 openat2 (437)；短期用 openat() 降级代替
  - **替代实现**（HarmonyOS 支持前的过渡方案） — ✔ 双轨验证通过（`j4_openat2_fb`）：

    ```c
    // 替代：openat() + O_NOFOLLOW + fstatat 路径段扫描模拟 RESOLVE_NO_SYMLINKS
    // 注：仍有 TOCTOU 窗口，安全性弱于 openat2 的原子语义
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <limits.h>
    #include <string.h>
    #include <errno.h>
    #include <unistd.h>
    
    int openat_no_symlinks(int dirfd, const char *path, int flags) {
        char buf[PATH_MAX];
        if (strlen(path) >= sizeof buf) { errno = ENAMETOOLONG; return -1; }
        strcpy(buf, path);
    
        // 校验路径中间每一段都不是 symlink
        for (char *p = buf + 1; (p = strchr(p, '/')); p++) {
            *p = '\0';
            struct stat st;
            if (fstatat(dirfd, buf, &st, AT_SYMLINK_NOFOLLOW) == 0
                && S_ISLNK(st.st_mode)) {
                errno = ELOOP;
                return -1;
            }
            *p = '/';
        }
        // 末段用 O_NOFOLLOW 防 symlink
        return openat(dirfd, path, flags | O_NOFOLLOW);
    }
    // TODO: 待 HarmonyOS 放行 openat2 (437) 后改用
    //   syscall(SYS_openat2, dirfd, path, &(struct open_how){.flags=...,
    //     .resolve=RESOLVE_NO_SYMLINKS}, sizeof(struct open_how))
    ```
- **`c5_fchmodat2`** — fchmodat2 (452) 带 flags 权限修改 — Bun add 权限
  - **建议：** 两端均不支持：容器无此 syscall (ENOSYS)，HarmonyOS 被 seccomp 拦截。向 HarmonyOS 申请放行
  - **替代实现**（HarmonyOS 支持前的过渡方案） — ✔ 双轨验证通过（`j12_fchmodat2_fb`）：

    ```c
    // 替代：经典 fchmodat() 无 flags 参数，丢失 AT_SYMLINK_NOFOLLOW 语义
    // 大多数 chmod 用例不需要 flags，可安全降级
    #include <fcntl.h>
    #include <sys/stat.h>
    
    int chmod_compat(int dirfd, const char *path, mode_t mode) {
        // 注意：丢失 AT_SYMLINK_NOFOLLOW；如需避免改 symlink 目标
        // 应先 fstatat 检查 S_ISLNK 再决定是否调用
        return fchmodat(dirfd, path, mode, 0);
    }
    // TODO: 待 HarmonyOS 放行 fchmodat2 (452) 后改用
    //       syscall(SYS_fchmodat2, dirfd, path, mode, flags)
    ```
- **`d6_memfd_dup2_fstat`** **[Bun]** — memfd_create + 子进程 dup2(memfd,1) + write，父 fstat 取回 st_size
  - **影响：** fork commit 0028 'Disable memfd for spawnSync on OHOS' 兜底为 socketpair。若本探针在设备侧失败（st_size=0），spawnSync 零拷贝 stdio 路径不可用，必须保留 socketpair fallback。
  - **触发路径：** `Bun.spawnSync() stdio 采集（零拷贝读取）:
  → bun_sys::memfd_create('bun-spawn-stdio', CLOEXEC|SEAL)  src/spawn_sys/spawn_process.rs:782
    → posix_spawn 子进程 dup2(memfd, STDOUT_FILENO)
      → 子进程 write(1, ...)
        → 父 waitpid + fstat(memfd, &st) → st.st_size == n`
  - **建议：** 若 OHOS quirk 复现（st_size=0），spawnSync stdio 走 socketpair；正常则可启用 memfd 零拷贝路径。
- **`g5_linkat_eperm`** **[Bun]** — linkat(AT_FDCWD) 在沙箱受限目录是否返 EPERM
  - **影响：** fork 在 src/install/{PackageManager,isolated_install/Hardlinker}.rs 加了 hardlink 失败回退到 copy 的兜底。若 EPERM 复现，bun install isolated 模式必须走 copy fallback，否则依赖去重断裂。
  - **触发路径：** `Bun isolated install / cache hardlink:
  → bun_install::Hardlinker::link(src, dst)             src/install/isolated_install/Hardlinker.rs
    → libc::linkat(AT_FDCWD, src, AT_FDCWD, dst, 0)
      → 内核 linkat(2)`
  - **建议：** 若复现，PackageInstaller hardlink 路径捕获 EPERM 并回退到 copy_file_range/read+write。
- **`g9_bun_cache_linkat`** **[Bun]** — $HOME/.bun/install/cache → store 真实路径 linkat 是否成功（补 g5 盲点）
  - **影响：** g5 只测 TMPDIR (/data/storage/el3/base)，真实 bun install 跑在 $HOME/.bun/install/{cache,store}。验证真实路径下 hardlink 是否仍 EACCES。
  - **触发路径：** `Bun install isolated 模式 cache → store:
  → bun_install::Hardlinker::link(cache_src, store_dst)  src/install/isolated_install/Hardlinker.rs
    → libc::linkat(AT_FDCWD, '$HOME/.bun/install/cache/...', AT_FDCWD, '...', 0)`
  - **建议：** Hardlinker 捕获 EACCES/EPERM/EXDEV 回退到 copy_file_range/read+write。
- **`k8_tmp_readonly`** **[Bun]** — /tmp 是否仍然是只读 erofs（resolver.rs 的三级 TMPDIR 回落链的存在前提）
  - **影响：** TMPDIR 解析目前要按 /tmp → /data/local/tmp → $HOME/tmp 顺序做运行时可写性探测；如果 /tmp 本身可写，前两级判断都是多余开销。
  - **触发路径：** `TMPDIR 解析 → #[cfg(all(not(android), ohos))] 运行时探测 /tmp 可写性（resolver/lib.rs:1714）`

### P2 · 影响工具链（6 项）

- **`a8_rseq`** — rseq (293) 可重启序列 — glibc 启动时注册（musl 不注册）
  - **影响：** rseq 是 Linux 5.1+ 内核特性，用于 per-CPU 无锁操作加速。仅 glibc (≥2.35) 在线程创建时自动注册 rseq；musl 不注册 rseq，Bun 不受影响。影响范围：glibc 链接的 Node.js/Deno 等运行时在 HarmonyOS 上可能触发 SIGSYS。
  - **触发路径：** `glibc 进程/线程启动
  → __init_tp / start_thread 自动注册 rseq
    → syscall(SYS_rseq=293, &rseq, sizeof rseq, 0, RSEQ_SIG)
      → seccomp filter → SIGSYS → 进程终止

注：Bun 使用 musl libc，musl 未实现 rseq 注册，因此 Bun 不受此影响。`
  - **建议：** 向 HarmonyOS 团队申请放行 rseq (293)；仅影响 glibc 工具链，不影响 musl/Bun
  - **替代实现**（HarmonyOS 支持前的过渡方案）：

    ```c
    // 替代：musl 启动早期 rseq 注册由 __init_tp 触发，应用层无法事先安装 handler
    // 推荐方案 A — 自编译 musl 时屏蔽 rseq 注册（最稳）：
    //
    //   --- a/src/thread/__init_tp.c
    //   +++ b/src/thread/__init_tp.c
    //   @@ rseq registration
    //   -    int r = __syscall(SYS_rseq, &td->rseq, sizeof td->rseq, 0, RSEQ_SIG);
    //   -    if (r) return r;
    //   +    /* HarmonyOS seccomp 拦截 rseq → 静默忽略，丢失 per-cpu 优化但保命 */
    //   +    (void)__syscall(SYS_rseq, &td->rseq, sizeof td->rseq, 0, RSEQ_SIG);
    //
    // 推荐方案 B — 应用层 SIGSYS 拦截（仅对 main 之后的 rseq 调用有效）：
    #define _GNU_SOURCE
    #include <signal.h>
    #include <sys/syscall.h>
    #include <ucontext.h>
    #include <errno.h>
    
    static void sigsys_to_enosys(int sig, siginfo_t *info, void *ctx) {
        if (info->si_syscall == 293 /* SYS_rseq */) {
            // aarch64: 让 syscall 返回 -ENOSYS，避免进程被 kill
            ((ucontext_t *)ctx)->uc_mcontext.regs[0] = -ENOSYS;
            return;
        }
        signal(sig, SIG_DFL); raise(sig);
    }
    
    __attribute__((constructor(101))) static void early_rseq_shield(void) {
        struct sigaction sa = { .sa_sigaction = sigsys_to_enosys,
                                .sa_flags = SA_SIGINFO | SA_NODEFER };
        sigaction(SIGSYS, &sa, NULL);
    }
    // TODO: 待 HarmonyOS 放行 rseq (293) 后移除 musl 补丁与 SIGSYS handler
    // 注意：方案 B 无法覆盖 musl 启动时 rseq 调用（早于 ctor 运行），必须配合方案 A
    ```
- **`d7_pidfd_poll_wait`** **[Bun]** — pidfd_open(X) + poll(POLLIN) + wait4(X, WNOHANG) 同 pid race 检测
  - **影响：** **HM 上观察到 race**：同 pid 上 pidfd POLLIN 触发后 wait4(WNOHANG) 仍返 0（task struct 可见性窗口）。但 fork spawn/process.rs:3266 的设计是 pidfd-on-PARENT + wait4-on-CHILD（不同 pid）+ wait4 flags=0（blocking），不受此 race 影响。本探针仅记录 OHOS 内核 quirk，不要求 bun 改动。
  - **触发路径：** `Bun.spawn() no_orphans 父死检测:
  → spawn/process.rs:3252 OHOS 分支
    → pidfd_open(ppid, 0)
    → poll([pidfd, stdio_fds], timeout)
    → wait4(pid, ..., WNOHANG)`
  - **建议：** 若未来有 pidfd+WNOHANG 同 pid 用法需要 retry 循环；当前 fork 实现规避了该模式。
- **`g1_tmpfile`** **[Bun]** — tmpfile() — musl libc / JSC 模块缓存依赖
  - **影响：** OHOS 移植版 musl 将 tmpfile() 默认路径硬编码为 /data/local/tmp，HarmonyOS 应用沙箱拒绝写入该路径（EACCES）。依赖 tmpfile() 的标准 C 程序无法创建临时文件。
  - **触发路径：** `musl libc tmpfile() 调用:
  → musl tmpfile() → openat(AT_FDCWD, P_tmpdir, O_RDWR|O_CREAT|O_EXCL)
    → P_tmpdir = "/data/local/tmp"
      → openat("/data/local/tmp/tmp-XXXXXX")
        → EACCES（HarmonyOS 应用沙箱拒绝写入）

WebKit JSC 临时文件:
  → mkostemp(template, O_CLOEXEC)                             FileSystemPOSIX.cpp:191
  → JSC 模块缓存 / 编译产物临时文件`
  - **建议：** 沙箱限制 P_tmpdir，改用 mkstemp() 在应用私有目录创建临时文件
  - **替代实现**（HarmonyOS 支持前的过渡方案） — ✔ 双轨验证通过（`j6_tmpfile_fb`）：

    ```c
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
    ```
- **`i9_getpwuid_r`** — getpwuid_r(geteuid()) 解析当前用户 — Node.js os.getUserInfo() 依赖
  - **影响：** Node.js os.getUserInfo() 内部调用 uv_os_get_passwd → getpwuid_r(geteuid())。HarmonyOS 沙箱为每个 HAP 分配动态 uid（2002xxxx），这些 uid 不在 /etc/passwd 中，getpwuid_r 返回 ENOENT，Node.js 抛出 'uv_os_get_passwd' 错误。
  - **触发路径：** `Node.js os.userInfo()
  → uv_os_get_passwd(&pwd)                                libuv/src/unix/core.c
    → uv__getpwuid_r(pwd, geteuid())                      libuv/src/unix/core.c
      → getpwuid_r(uid, &pw, buf, sz, &result)            libc (POSIX)
        → uid=20020101 not in /etc/passwd → ENOENT`
  - **建议：** HarmonyOS 应用 uid 不在 /etc/passwd 中，用 getenv("HOME") 替代 homedir，用 getlogin_r 或预留 fallback
  - **替代实现**（HarmonyOS 支持前的过渡方案）：

    ```c
    // Node.js os.userInfo() 替代：用 OHOS 应用环境变量组装用户信息
    // 适用场景：HarmonyOS 原生进程 (非 HAP)，uid 不在 /etc/passwd 中
    #include <unistd.h>
    #include <stdio.h>
    #include <stdlib.h>
    
    typedef struct {
        unsigned int uid, gid;
        const char *username, *homedir, *shell;
    } user_info_t;
    
    void get_user_info_fallback(user_info_t *info) {
        info->uid = geteuid();
        info->gid = getegid();
    
        // username: 优先 LOGNAME/USER，否则用 uid 的十进制字符串
        info->username = getenv("LOGNAME");
        if (!info->username) info->username = getenv("USER");
        if (!info->username) {
            static char name[32];
            snprintf(name, sizeof(name), "u%u", info->uid);
            info->username = name;
        }
    
        // homedir: 优先 HOME，否则 /data/storage/el2/base
        info->homedir = getenv("HOME");
        if (!info->homedir) info->homedir = "/data/storage/el2/base";
    
        // shell: 总是不可用（HarmonyOS app 无登录 shell）
        info->shell = "/bin/false";
    }
    // TODO: 待 HarmonyOS 将 uid 加入 /etc/passwd 或实现 nss_ohos 后移除
    ```
- **`k6_statx_socket_fd`** **[Bun]** — statx(2) 对 socket-backed fd 是否仍然返回 EBADF（c7_statx 只测普通文件路径）
  - **影响：** statx 在通用文件路径上可用（见 c7_statx），但对 socket fd 的这个更具体的失败形状只有 OHOS 独有，需要单独的 errno 特判才能正确回退到 fstat。
  - **触发路径：** `sys/lib.rs statx 包装 → 对 socket fd 调用返回 EBADF → cfg!(ohos) 特判折进 statx_fallback（sys/lib.rs:2358）`
- **`k12_bind_privileged_port`** **[Bun]** — 不带 CAP_NET_BIND_SERVICE 时能否 bind() 特权端口（<1024）
  - **影响：** Bun cluster 模块的特权端口监听（如 80/443）目前在这台设备上完全不可用，相关测试被跳过。
  - **触发路径：** `net.Server.listen(<1024>) / cluster 主进程转发 fd → bind() → 缺 CAP_NET_BIND_SERVICE → EACCES`

## 🟡 异常 — HM 通过但 OH 失败

| 探针 | 描述 | OH | HM | 建议 |
|------|------|-----|-----|------|
| `k1_exec_selfsigned_elf` | 未签名/刚落盘的 ELF 拷贝能否 exec（不同于 l0/03：这份拷贝是全新 inode，模拟刚解包的产物） | 不适用 | 通过 | - |
| `k2_exec_shebang_script` | 内核 binfmt_script 直接 execve() 一个 #! 脚本是否会被展开（不经过手工 shebang 解析兜底） | 不适用 | 通过 | - |
| `k10_reflink_residue` | copy_file_range 克隆文件后不 ftruncate/fsync 直接覆写，独立 fd 读到的是否仍是新内容（而非残留的克隆数据） | 不适用 | 通过 | - |

## ⚪ 两端共同失败

| 探针 | 描述 | OH | HM | 建议 |
|------|------|-----|-----|------|
| `k7_rlimit_nofile_default` | RLIMIT_NOFILE 默认软上限是否仍然低于 163840（ohos-bun 强制拉高的阈值） | 失败 | 失败 | - |

## 🔧 无法测定 — 前置条件不满足（exit 2），先修环境再下结论

| 探针 | 描述 | OH | HM | 建议 |
|------|------|-----|-----|------|
| `k5_open_oexec_xbit` | open(path, O_EXEC) 是否仍然不校验文件的可执行权限位（0660 文件应被拒绝却被放行） | 不适用 | 失败 | - |
| `k9_execonly_elf_read` | chmod 0111（无读位）的 ELF 能否通过 open("/proc/self/exe") 读取自身 | 不适用 | 失败 | - |
| `k11_dlopen_unsigned_so` | 内核是否仍然拒绝 dlopen() 一个 .codesign 段内容已失效（签名破坏）的 .so | 不适用 | 失败 | - |
