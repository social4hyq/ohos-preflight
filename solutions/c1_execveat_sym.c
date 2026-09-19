#include <sys/syscall.h>
#include <fcntl.h>

int main(void) {
    char *argv[] = {"/bin/true", NULL};
    // libc 未导出 execveat → 用 syscall() 直接调用
    long ret = syscall(SYS_execveat, AT_FDCWD,
                       "/bin/true", argv, NULL, 0);
    return (ret == -1 && errno == ENOSYS) ? 1 : 0;
}
