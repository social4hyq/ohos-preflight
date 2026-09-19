// probes/k13_getaddrinfo_netsys.c
//
// 【测试目的】验证系统 getaddrinfo()（走 netsys IPC 私有通道）是否仍然
// 是唯一可用的域名解析路径。
//
// 【关联项目】ohos-bun
//
// 【上游调用链】
// Bun DNS 解析（dns.lookup 默认路径，r34 起改为走系统解析器）:
//   → #[cfg(any(android, ohos))]: 使用系统 getaddrinfo()             dns/lib.rs:281-294
//     → 系统 getaddrinfo() 内部走 netsys 私有 IPC 通道
//       → 而不是 Bun 自带的、直接发 UDP/TCP 到 /etc/resolv.conf
//         里 nameserver 的用户态 DNS 客户端
//
// 【为何需要】这条分支意味着 Bun 在 OHOS 上放弃了自己的 DNS 客户端实现，
// 完全依赖系统服务；如果 netsys 服务不可达（比如 devmode 关闭后系统服务
// 权限收紧），DNS 解析会失败且没有独立的用户态兜底路径。
//
// Exit: 0=pass (系统 getaddrinfo 解析外部域名成功 -- netsys 通道可用)
//       1=fail (loopback 解析正常但外部域名解析失败 -- netsys 通道本身
//              出了问题，不是单纯网络不通)
//       2=unsupported (连 loopback 都解析不了，判断不出 netsys 状态，
//              也可能是这台设备当前完全没有网络)

#define _GNU_SOURCE
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static volatile int timed_out = 0;
static void handle_alarm(int sig) { timed_out = 1; }

static int resolve(const char *host) {
    struct addrinfo hints = { .ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM };
    struct addrinfo *res = NULL;
    int rc = getaddrinfo(host, NULL, &hints, &res);
    if (res) freeaddrinfo(res);
    return rc;
}

int main(void) {
    signal(SIGALRM, handle_alarm);

    if (resolve("localhost") != 0) {
        fprintf(stderr, "getaddrinfo(localhost) failed -- can't establish a resolver baseline (probably no network stack at all)");
        return 2;
    }

    alarm(8);
    int rc = resolve("connectivitycheck.platform.hicloud.com");
    alarm(0);

    if (timed_out) {
        fprintf(stderr, "getaddrinfo(external host) timed out after 8s -- netsys channel appears stuck");
        return 1;
    }
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo(external host) failed: %s -- netsys channel or DNS reachability broken", gai_strerror(rc));
        return 1;
    }
    return 0;
}
