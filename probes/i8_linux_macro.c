// probes/i8_linux_macro.c
//
// 【测试目的】__linux__ 预处理器宏是否定义。
//
// 【关联项目】Bun / WebKit
//
// 【上游调用链】
// Bun C/C++ 依赖编译时平台检测 (WebKit/BoringSSL/libuv/mimalloc):
//   → #ifdef __linux__ / #if OS(LINUX)                           CMakeLists.txt / Platform.h
//   → Rust 侧: cfg(target_os = "linux") → linux_syscall.rs
//   → Zig 侧: builtin.os.tag == .linux → sys.zig
//   → WebKit OptionsCommon.cmake:317 — -D_GNU_SOURCE=1 依赖 __linux__
//
#include <stdio.h>

int main(void) {
#ifdef __linux__
    return 0;
#else
    fprintf(stderr, "__linux__ not defined; only __OHOS__ is available");
    return 1;
#endif
}

