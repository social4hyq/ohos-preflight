// probes/d1_shm.c
//
// 【测试目的】shm_open/mmap 创建 POSIX 共享内存 — WebKit SharedMemoryUnix 三级后备链。
//
// 【关联项目】Bun / WebKit
//
// 【上游调用链】
// WebKit 进程间共享内存 (Bun 静态链接):
//   → syscall(__NR_memfd_create, "WebKitSharedMemory", MFD_CLOEXEC)   SharedMemoryUnix.cpp:87
//     → ENOSYS? → shm_open(SHM_ANON, ...)                             同文件:102
//       → 不可用? → shm_open(tempName, O_CREAT | O_RDWR, ...)         同文件:111
//         → ftruncate + mmap(MAP_SHARED)
//
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void) {
    const char *name = "/harmonybrew_probe_d1";
    shm_unlink(name);  // ignore failure; previous run may not exist
    int fd = shm_open(name, O_CREAT | O_RDWR, 0600);
    if (fd < 0) {
        perror("shm_open");
        return 1;
    }
    if (ftruncate(fd, 8) < 0) {
        perror("ftruncate");
        close(fd);
        shm_unlink(name);
        return 1;
    }
    void *p = mmap(NULL, 8, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        close(fd);
        shm_unlink(name);
        return 2;
    }
    memcpy(p, "HRBYPROB", 8);
    char buf[8];
    memcpy(buf, p, 8);
    int ok = memcmp(buf, "HRBYPROB", 8) == 0;
    munmap(p, 8);
    close(fd);
    shm_unlink(name);
    if (!ok) {
        fprintf(stderr, "shm read mismatch\n");
        return 1;
    }
    return 0;
}

