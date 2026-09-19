// 替代：TCP loopback 替代 Unix 域套接字用于本机 IPC
// 局限：无 SCM_RIGHTS fd 传递；性能略低于 Unix socket
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>

int create_loopback_server(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK)
    };
    bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(fd, 5);
    return fd;
}
// TODO: 待 HarmonyOS 放行 Unix socket bind() 后切回 AF_UNIX
