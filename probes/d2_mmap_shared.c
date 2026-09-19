// probes/d2_mmap_shared.c
//
// 【测试目的】MAP_SHARED 匿名映射 — JSC SharedArrayBuffer 依赖。
//
// 【关联项目】Bun / WebKit
//
// 【上游调用链】
// JSC SharedArrayBuffer & WebKit 进程间共享内存 (Bun 静态链接):
//   → mmap(NULL, len, PROT_READ|PROT_WRITE,
//       MAP_SHARED|MAP_ANONYMOUS, -1, 0)                      OSAllocatorPOSIX.cpp:231
//   → WebKit SharedMemoryUnix.cpp:135,152 — mmap(MAP_SHARED)
//   → JSC 堆管理 — MAP_SHARED 用于共享内存 GC 堆
//
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    unsigned char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                            MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        return 2;
    }
    p[0] = 0;
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        munmap(p, 4096);
        return 2;
    }
    if (pid == 0) {
        p[0] = 0xAB;
        _exit(0);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        munmap(p, 4096);
        return 2;
    }
    unsigned char v = p[0];
    munmap(p, 4096);
    if (v != 0xAB) {
        fprintf(stderr, "MAP_SHARED across fork failed: got 0x%02x\n", v);
        return 1;
    }
    return 0;
}

