// probes/i17_openpty_libc.c
//
// 【测试目的】openpty() 符号在 OHOS 是否随 libc.so 直接可用（无 libutil）。
//
// 【关联项目】Bun
//
// 【上游调用链】
// Bun Terminal openpty 动态加载:
//   → Terminal::get_open_pty_fn()                              src/runtime/api/bun/Terminal.rs:840
//     → dlopen("libutil.so" | "libutil.so.1" | "libc.so.6" | "libc.so")
//       → dlsym(handle, "openpty")
//
// 【为何需要】fork 在 Terminal.rs 加 cfg(target_env="ohos") 直接 extern "C" 链 libc 的 openpty
// （绕开 dlopen("libutil.so") — OHOS 无 libutil）。本探针验证 dlopen("libc.so")+dlsym("openpty") 路径是否可用。
//
// 【影响】若失败：fork 直接 link openpty 是必需的；若成功：fork 可简化为只走 libc.so fallback。
//
// Exit: 0=pass (openpty 可解析), 1=fail

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *libs[] = { "libutil.so", "libutil.so.1", "libc.so.6", "libc.so", "libc.musl-aarch64.so.1", NULL };
    void *openpty_sym = NULL;
    const char *via = NULL;

    for (int i = 0; libs[i]; i++) {
        void *h = dlopen(libs[i], RTLD_LAZY);
        if (!h) continue;
        void *s = dlsym(h, "openpty");
        if (s) {
            openpty_sym = s;
            via = libs[i];
            dlclose(h);
            break;
        }
        dlclose(h);
    }

    if (!openpty_sym) {
        void *s = dlsym(RTLD_DEFAULT, "openpty");
        if (s) {
            openpty_sym = s;
            via = "RTLD_DEFAULT";
        }
    }

    if (!openpty_sym) {
        fprintf(stderr, "openpty not resolvable via any libutil/libc handle: %s", dlerror());
        return 1;
    }

    fprintf(stderr, "openpty resolved via %s — Terminal.rs OHOS direct-link branch can be simplified", via);
    return 0;
}
