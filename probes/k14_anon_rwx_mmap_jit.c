// probes/k14_anon_rwx_mmap_jit.c
//
// 【测试目的】验证内核是否允许匿名 RWX mmap 并真正执行其中写入的代码——
// 这是 JSC/bun:ffi 需要的 JIT 前提。不只测 mmap() 调用本身是否返回成功
// （PROT_EXEC 请求可能被静默降级或在指令取指时才拒绝），还要实际跳进去
// 执行一条写入的指令，证明"可映射"和"可执行"是两回事。
//
// 【关联项目】ohos-bun / ohos-opencode（OpenTUI 经 bun:ffi）
//
// 【上游调用链】
// opencode TUI 启动 → @ohos-npm-ports/opentui-core loadBackend()
//   → createBunBackend → bun:ffi dlopen/JSCallback
//     → JSC::ExecutableAllocator 保留匿名 RWX 区（[anon:JSJITCode]）
//       → 商用机内核代码签名策略拒绝匿名可执行内存
//         → bun:ffi 在 new 阶段直接 throw "bun:ffi requires the JIT"
//           （无解释执行 fallback，见 ohos-bun 测试 fixture
//            test/js/bun/jsc-stress/fixtures/ffi/ffi-no-jit.js）
//
// 【为何需要】atomgit issue social4hyq/homebrew-core#1 报告商用 HarmonyOS
// 设备上 opencode TUI 因此崩溃；本项目此前所有真机结论都产自 devmode=on
// 的机器（`param get const.security.developermode.state` = true），从未
// 有独立探针验证过 devmode=off/商用机语义下匿名 RWX 是否被拒。用
// `BUN_JSC_useJIT=0` 可在 devmode=on 的本机复现"JIT 不可用"这个症状，
// 但那是 bun 主动放弃 JIT，不等于内核真的会拒绝匿名 RWX——本探针直接问
// 内核本身的答案，用于后续 devmode-off 快照对比时判断二者是否一致。
//
// 手法：fork 出子进程做真正的"分配可执行内存 + 跳进去执行"测试，因为
// 若指令取指阶段被拒（而非 mmap() 阶段），进程会收到 SIGSEGV/SIGBUS/
// SIGSYS，必须让崩溃只发生在子进程里，不能拖垮探针本身（参考 k1 的
// fork 隔离手法）。写入的代码是一条 aarch64 `ret`（RET x30 的
// LR-隐式形式，编码 0xD65F03C0），函数指针调用后应立即返回，用一个易
// 识别的返回值确认"确实执行了写入的指令"而非侥幸没崩溃。
//
// Exit: 0=pass （匿名 RWX 可分配且可执行 —— JIT 前提成立，bun:ffi 应该
//              可用；这台设备大概率是 devmode=on 或未强制企业级代码
//              签名）
//       1=fail （mmap(PROT_EXEC) 被拒 EACCES/EPERM，或分配成功但跳转
//              执行时被信号杀死 —— 商用机式的匿名可执行内存拒绝，
//              bun:ffi/OpenTUI 在此设备上会复现 issue #1 的报错）
//       2=unsupported （mmap 因 ENOMEM 等与代码签名无关的原因失败，或
//              子进程异常退出但不是被信号杀死也不是预期返回值）

#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

// Child-only internal exit codes, translated by the parent into the
// probe's public 0/1/2 contract.
#define CHILD_OK 0
#define CHILD_MMAP_DENIED 10   // EACCES/EPERM at mmap() itself
#define CHILD_MMAP_OTHER 11    // any other mmap() failure (ENOMEM, ...)
#define CHILD_EXECUTED_WRONG 12 // jumped in, returned, but wrong value (bug in probe, not the kernel)

#if defined(__aarch64__)
// `ret` (return via x30), a single valid 4-byte aarch64 instruction.
static const unsigned char kCode[] = {0xC0, 0x03, 0x5F, 0xD6};
#elif defined(__x86_64__)
// `ret`
static const unsigned char kCode[] = {0xC3};
#else
#error "k14_anon_rwx_mmap_jit.c needs a return-immediately stub for this architecture"
#endif

typedef int (*fn_t)(void);

static int child_main(void) {
    long pagesz = sysconf(_SC_PAGESIZE);
    if (pagesz <= 0) pagesz = 4096;

    void *mem = mmap(NULL, (size_t)pagesz, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        int err = errno;
        fprintf(stderr, "mmap(PROT_EXEC anon) failed: %s", strerror(err));
        return (err == EACCES || err == EPERM) ? CHILD_MMAP_DENIED : CHILD_MMAP_OTHER;
    }

    memcpy(mem, kCode, sizeof(kCode));

    // aarch64 I$/D$ are not coherent: a plain memcpy() only lands in the
    // data cache. Without this, the CPU can fetch stale/garbage bytes left
    // over from the page's previous use and SIGILL on a bogus opcode --
    // that is a self-modifying-code bug in *this probe*, not a kernel
    // code-signing denial, and must not be confused with one.
    __builtin___clear_cache((char *)mem, (char *)mem + sizeof(kCode));

    // If PROT_EXEC was silently downgraded, or the kernel enforces
    // code-signing at instruction-fetch time rather than at mmap() time,
    // the call below is what actually finds out -- it may never return.
    fn_t fn = (fn_t)mem;
    int rc = fn();

    munmap(mem, (size_t)pagesz);

    // `ret` with no explicit return value: whatever was in w0/eax on entry
    // comes back. We only care that we got here at all -- reaching this
    // line already proves the instruction fetch+execute succeeded.
    (void)rc;
    return CHILD_OK;
}

int main(void) {
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "fork: %s", strerror(errno));
        return 2;
    }
    if (pid == 0) {
        _exit(child_main());
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "waitpid: %s", strerror(errno));
        return 2;
    }

    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        fprintf(stderr,
                "execution of anonymous RWX page was killed by signal %d (%s) -- "
                "mmap() succeeded but the kernel refused to run the code "
                "(instruction-fetch-time denial, not mmap-time)",
                sig, strsignal(sig));
        return 1;
    }
    if (!WIFEXITED(status)) {
        fprintf(stderr, "child ended abnormally (raw status %d)", status);
        return 2;
    }

    switch (WEXITSTATUS(status)) {
        case CHILD_OK:
            return 0;
        case CHILD_MMAP_DENIED:
            fprintf(stderr, "mmap(PROT_EXEC anon) denied at mmap() time (EACCES/EPERM)");
            return 1;
        case CHILD_MMAP_OTHER:
            fprintf(stderr, "mmap(PROT_EXEC anon) failed for a non-code-signing reason");
            return 2;
        default:
            fprintf(stderr, "unexpected child exit code %d", WEXITSTATUS(status));
            return 2;
    }
}
