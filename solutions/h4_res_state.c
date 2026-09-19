#include <sys/socket.h>
#include <netdb.h>

// _res 全局变量不可用 → 改用 getaddrinfo()
struct addrinfo hints = {.ai_family = AF_UNSPEC};
struct addrinfo *res;
int ret = getaddrinfo("localhost", NULL, &hints, &res);
if (ret == 0) freeaddrinfo(res);
