// probes/_os_version.c
//
// 运行时 OS 版本探测，输出 JSON。被 run-dual.sh 用来采集环境元数据。
// 不参与能力对比（run.sh 跳过 _ 前缀的二进制文件）。
//
// 检测方式：
//   编译时：__OHOS__ 宏（OHOS NDK target 必定定义）
//   运行时：dlopen libc.musl-*.so.1 成功 = musl（OHOS），失败 = glibc（普通 Linux）
//   内核信息：uname(2)
//   API 级别：get_device_api_version()（OHOS API 12+，可能返回 0）
#include <stdio.h>
#include <sys/utsname.h>
#include <dlfcn.h>
#include <string.h>

#ifdef __OHOS__
#include <info/device_api_version.h>
#include <info/application_target_sdk_version.h>
#endif

static int is_musl(void) {
    // musl 动态链接器命名: ld-musl-<arch>.so.1
    void *h = dlopen("libc.musl-aarch64.so.1", RTLD_LAZY);
    if (h) { dlclose(h); return 1; }
    h = dlopen("libc.musl-x86_64.so.1", RTLD_LAZY);
    if (h) { dlclose(h); return 1; }
    return 0;
}

int main(void) {
    struct utsname u;
    if (uname(&u) != 0) {
        printf("{\"error\":\"uname failed\"}\n");
        return 0;
    }

    printf("{\"sysname\":\"%s\"", u.sysname);
    printf(",\"release\":\"%s\"", u.release);
    printf(",\"version\":\"%s\"", u.version);
    printf(",\"machine\":\"%s\"", u.machine);

#ifdef __OHOS__
    printf(",\"is_ohos\":true");
    printf(",\"api_level\":%d", get_device_api_version());
    printf(",\"target_sdk\":%d", get_application_target_sdk_version());
    printf(",\"oh_current_api\":%d", OH_CURRENT_API_VERSION);
#else
    printf(",\"is_ohos\":%s", is_musl() ? "true" : "false");
#endif

    printf("}\n");
    return 0;
}
