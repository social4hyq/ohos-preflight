// probes/d4_memfd.c
//
// 【测试目的】memfd_create (279) 内存文件 — Bun add 触发。
//
// 【关联项目】Bun / WebKit
//
// 【上游调用链】
// Bun.spawn() stdio 管道替代方案:
//   → bun_sys::memfd_create("bun-spawn-stdio",                 bun/src/spawn_sys/spawn_process.rs:782
//       MemfdFlags::CrossProcess)
//     → 内核 memfd_create(2)
// 
// WebKit 共享内存 & seccomp BPF:
//   → syscall(__NR_memfd_create, "WebKitSharedMemory",          SharedMemoryUnix.cpp:87
//       MFD_CLOEXEC)
//   → syscall(__NR_memfd_create, ..., MFD_CLOEXEC|MFD_ALLOW_SEALING)
//       (seccomp BPF + argument passing)                        BubblewrapLauncher.cpp:68
//
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#ifndef SYS_memfd_create
#define SYS_memfd_create 279
#endif
#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif
#ifndef MFD_ALLOW_SEALING
#define MFD_ALLOW_SEALING 0x0002U
#endif

int main(void) {
    // Bun: memfd_create("bun-spawn-stdio", MFD_CLOEXEC | MFD_ALLOW_SEALING)
    int fd = (int)syscall(SYS_memfd_create, "bun-spawn-stdio",
                          MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0) {
        fprintf(stderr, "memfd_create failed: %s\n", strerror(errno));
        return 1;
    }

    // Verify it's a usable fd by writing to it
    if (write(fd, "probe", 5) != 5) {
        fprintf(stderr, "memfd write failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }
    close(fd);
    return 0;
}

