// solutions/h4_getaddrinfo.c
//
// 替代 _res 全局变量：使用 getaddrinfo() 进行 DNS 查询。
// OHOS musl 不导出 _res，应使用标准 getaddrinfo() API。
//
// 验证：对 localhost 执行 getaddrinfo 查询，验证返回结果。

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>

int main(void) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int ret = getaddrinfo("localhost", NULL, &hints, &res);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo failed: %s", gai_strerror(ret));
        return 1;
    }
    freeaddrinfo(res);
    return 0;
}
