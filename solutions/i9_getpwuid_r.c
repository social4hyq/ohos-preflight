// Node.js os.userInfo() 替代：用 OHOS 应用环境变量组装用户信息
// 适用场景：HarmonyOS 原生进程 (非 HAP)，uid 不在 /etc/passwd 中
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    unsigned int uid, gid;
    const char *username, *homedir, *shell;
} user_info_t;

void get_user_info_fallback(user_info_t *info) {
    info->uid = geteuid();
    info->gid = getegid();

    // username: 优先 LOGNAME/USER，否则用 uid 的十进制字符串
    info->username = getenv("LOGNAME");
    if (!info->username) info->username = getenv("USER");
    if (!info->username) {
        static char name[32];
        snprintf(name, sizeof(name), "u%u", info->uid);
        info->username = name;
    }

    // homedir: 优先 HOME，否则 /data/storage/el2/base
    info->homedir = getenv("HOME");
    if (!info->homedir) info->homedir = "/data/storage/el2/base";

    // shell: 总是不可用（HarmonyOS app 无登录 shell）
    info->shell = "/bin/false";
}
// TODO: 待 HarmonyOS 将 uid 加入 /etc/passwd 或实现 nss_ohos 后移除
