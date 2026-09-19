// probes/a6_bpf.c
//
// 【测试目的】验证 BPF(BPF_MAP_CREATE) 系统调用可用性。
// BPF 是 seccomp 安全机制的底层依赖：seccomp filter 必须先编译为 BPF
// bytecode，再由内核 BPF 验证器校验后加载到 seccomp hook 点。
//
// 【上游调用链】vite-plus
//   seccompiler (rust-vmm/seccompiler)     ← 编译 seccomp rule → BPF bytecode
//     → bpf(BPF_MAP_CREATE, ...)            ← 创建 BPF map 存储 filter 数据
//       → vite-task/crates/fspy_seccomp_unotify ← 使用 BPF+seccomp 拦截 syscall
//
// 【影响】vite-plus 的 fspy 模块依赖 seccomp + BPF 实现文件系统操作拦截。
// HarmonyOS 上 bpf() 返回 EPERM，fspy 的 syscall 拦截功能完全不可用。
//
// Exit: 0=pass (BPF map created), 1=fail (EPERM/ENOSYS)

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef SYS_bpf
#define SYS_bpf 280
#endif

#ifndef BPF_MAP_CREATE
#define BPF_MAP_CREATE 0
#endif

#ifndef BPF_MAP_TYPE_ARRAY
#define BPF_MAP_TYPE_ARRAY 2
#endif

union bpf_attr {
    struct {
        __UINT32_TYPE__ map_type;
        __UINT32_TYPE__ key_size;
        __UINT32_TYPE__ value_size;
        __UINT32_TYPE__ max_entries;
        __UINT32_TYPE__ map_flags;
    };
};

int main(void) {
    union bpf_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.map_type = BPF_MAP_TYPE_ARRAY;
    attr.key_size = 4;
    attr.value_size = 4;
    attr.max_entries = 1;
    long ret = syscall(SYS_bpf, BPF_MAP_CREATE, &attr, sizeof(attr));
    if (ret < 0) {
        perror("bpf");
        return 1;
    }
    close((int)ret);
    return 0;
}
