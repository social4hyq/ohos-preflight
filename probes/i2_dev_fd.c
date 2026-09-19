// probes/i2_dev_fd.c
//
// 【测试目的】/dev/fd 与 /proc/self/fd 路径可用性。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

int main(void) {
    char path[64];
    snprintf(path, sizeof(path), "/dev/fd/%d", STDOUT_FILENO);

    // Check if we can stat the /dev/fd entry for our own stdout
    if (access(path, F_OK) == 0) {
        return 0;
    }
    int dev_errno = errno;

    snprintf(path, sizeof(path), "/proc/self/fd/%d", STDOUT_FILENO);
    if (access(path, F_OK) == 0) {
        fprintf(stderr, "/dev/fd not available (errno=%d), /proc/self/fd is the fallback", dev_errno);
        return 1;
    }
    int proc_errno = errno;

    fprintf(stderr, "neither /dev/fd (errno=%d) nor /proc/self/fd (errno=%d) accessible", dev_errno, proc_errno);
    return 2;
}

