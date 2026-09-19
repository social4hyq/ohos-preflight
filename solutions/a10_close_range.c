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
