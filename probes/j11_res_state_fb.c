// probes/j11_res_state_fb.c
//
// 【测试目的】h4_res_state 替代：getaddrinfo() 替代 _res 全局状态。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#define _GNU_SOURCE
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

int main(void) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = NULL;
    int rc = getaddrinfo("localhost", NULL, &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo(localhost): %s", gai_strerror(rc));
        return 1;
    }
    int ok = 0;
    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        if (ai->ai_family == AF_INET || ai->ai_family == AF_INET6) {
            ok = 1;
            break;
        }
    }
    freeaddrinfo(res);
    if (ok) return 0;
    fprintf(stderr, "no inet sockaddr returned for localhost");
    return 1;
}

