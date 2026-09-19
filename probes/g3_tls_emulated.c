// probes/g3_tls_emulated.c
//
// 【测试目的】__thread TLS — Zig 编译目标 OHOS TLS 实现检测。
//
// 【关联项目】Bun / WebKit
//
// 【上游调用链】
// Bun/Zig OHOS 目标 TLS ABI 检测 & WebKit 线程局部存储:
//   → __thread int tls_val = 0; 校验线程间地址不同
//   → WebKit ThreadingPOSIX.cpp — _pthread_setspecific_direct (fast TLS)
//   → Bun __cxa_thread_atexit_impl — glibc 兼容层线程退出回调
//   → Zig OHOS 目标编译时检测: emulated TLS vs ELF native TLS ABI
//
#include <stdio.h>
#include <pthread.h>
#include <string.h>

static __thread int tls_val = 0;

struct tls_ctx {
    int *expected_addr;
    int result;
};

static void *check_tls(void *arg) {
    struct tls_ctx *ctx = (struct tls_ctx *)arg;
    tls_val = 42;
    ctx->result = (&tls_val != ctx->expected_addr && tls_val == 42);
    return NULL;
}

int main(void) {
    pthread_t tid;
    struct tls_ctx ctx = { .expected_addr = &tls_val, .result = 0 };

    tls_val = 7;

    if (pthread_create(&tid, NULL, check_tls, &ctx) != 0) {
        fprintf(stderr, "pthread_create failed");
        return 2;
    }
    pthread_join(tid, NULL);

    // main thread still sees 7
    if (tls_val != 7) {
        fprintf(stderr, "main thread TLS corrupted: expected 7 got %d", tls_val);
        return 1;
    }
    if (!ctx.result) {
        fprintf(stderr, "__thread variable shared across threads (emulated TLS or broken)");
        return 1;
    }
    return 0;
}

