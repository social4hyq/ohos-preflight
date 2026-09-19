// probes/h3_program_invocation.c
//
// 【测试目的】program_invocation_name 全局变量。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#include <stdio.h>
#include <dlfcn.h>
#include <string.h>

int main(void) {
    // Try musl-compatible __progname first (available on musl, BSD)
    const char **progname = (const char **)dlsym(RTLD_DEFAULT, "__progname");
    if (progname && *progname) {
        return 0;
    }

    // Try glibc program_invocation_name
    char **pin = (char **)dlsym(RTLD_DEFAULT, "program_invocation_name");
    if (pin && *pin) {
        return 0;
    }

    // Try program_invocation_short_name as last resort
    char **pisn = (char **)dlsym(RTLD_DEFAULT, "program_invocation_short_name");
    if (pisn && *pisn) {
        return 0;
    }

    fprintf(stderr, "neither __progname nor program_invocation_name available: %s", dlerror());
    return 1;
}

