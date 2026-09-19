// probes/j13_shim.c
//
// Built into probes/_j13shim.so (leading underscore so run.sh skips it).
// LD_PRELOAD shim for j13_seccomp_shim_fb: interposes libc open(), appends
// the intercepted path to $J13_LOG, then forwards via RTLD_NEXT — the
// libc-level audit pattern proposed as the a1_seccomp_unotify fallback.
//
// The log is written with raw syscalls (SYS_openat/write) so the shim never
// re-enters its own open() interposer.

#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

static void log_path(const char *path) {
    const char *logfile = getenv("J13_LOG");
    if (!logfile) return;
    long fd = syscall(SYS_openat, AT_FDCWD, logfile,
                      O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;
    syscall(SYS_write, fd, path, strlen(path));
    syscall(SYS_write, fd, "\n", 1);
    syscall(SYS_close, fd);
}

typedef int (*orig_open_t)(const char *, int, ...);

int open(const char *path, int flags, ...) {
    static orig_open_t real;
    if (!real) real = (orig_open_t)dlsym(RTLD_NEXT, "open");

    log_path(path);

    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode_t mode = va_arg(ap, mode_t);
        va_end(ap);
        return real(path, flags, mode);
    }
    return real(path, flags);
}
