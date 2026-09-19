// probes/i9_getpwuid_r.c
//
// 【测试目的】验证 getpwuid_r(geteuid()) 可用性。
// getpwuid_r 通过系统密码数据库（/etc/passwd）解析当前用户信息。
//
// 【上游调用链】NodeJS (os.userInfo())
//   Node.js os.userInfo()
//     → uv_os_get_passwd(&pwd)                              libuv/src/unix/core.c
//       → getpwuid_r(uid, &pw, buf, sz, &result)            libc (POSIX)
//         → uid=2002xxxx not in /etc/passwd → ENOENT
//
// 【影响】HarmonyOS 沙箱为每个 HAP 分配动态 uid（2002xxxx），这些 uid
// 不在 /etc/passwd 中。Node.js os.userInfo() 抛出 'uv_os_get_passwd' 错误。
// 降级方案：用 getenv("HOME") + getenv("LOGNAME") 组装用户信息。
//
// Exit codes:
//   0 = user resolved (name, dir, shell populated)
//   1 = user not found or other error

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>

int main(void) {
    struct passwd pwd, *result = NULL;
    char buf[4096];
    int ret = getpwuid_r(geteuid(), &pwd, buf, sizeof(buf), &result);
    if (ret != 0 || result == NULL) {
        fprintf(stderr, "getpwuid_r: %s", ret != 0 ? strerror(ret) : "no entry");
        return 1;
    }
    if (!result->pw_name || !result->pw_dir) {
        fprintf(stderr, "getpwuid_r: incomplete entry");
        return 1;
    }
    return 0;
}
