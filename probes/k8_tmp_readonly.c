// probes/k8_tmp_readonly.c
//
// 【测试目的】验证 /tmp 是否仍然是只读 erofs，判断 resolver.rs 里
// "/tmp 不可写 → /data/local/tmp → $HOME/tmp" 的三级回落链是否还有
// 必要保留前两级。
//
// 【关联项目】ohos-bun
//
// 【上游调用链】
// TMPDIR 解析（首次需要临时目录时）:
//   → 尝试 /tmp                                              resolver/lib.rs:1714
//     → #[cfg(ohos)]: 运行时探测可写性，不可写就跳过
//       → 依次尝试 /data/local/tmp、$HOME/tmp
//
// Exit: 0=pass (/tmp 可写 — 三级回落链的存在意义消失，可以直接用 /tmp)
//       1=fail (/tmp 仍然只读 — 回落链仍然必需)
//       2=unsupported (/tmp 路径本身不存在)

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void) {
    struct stat st;
    if (stat("/tmp", &st) != 0) {
        fprintf(stderr, "stat(/tmp): %s", strerror(errno));
        return 2;
    }

    char path[64];
    snprintf(path, sizeof(path), "/tmp/.ohos-preflight-k8-%d", getpid());
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) {
        fprintf(stderr, "/tmp not writable: %s", strerror(errno));
        return 1;
    }
    close(fd);
    unlink(path);
    return 0;
}
