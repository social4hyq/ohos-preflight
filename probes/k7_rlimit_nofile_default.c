// probes/k7_rlimit_nofile_default.c
//
// 【测试目的】测出这台设备 RLIMIT_NOFILE 的默认软/硬上限，判断
// ohos-bun 强制拉高到 163840 的手工调整是否还有必要。
//
// 【关联项目】ohos-bun
//
// 【上游调用链】
// adjust_ulimit()（启动早期调用一次）:
//   → getrlimit(RLIMIT_NOFILE)                                resolver/lib.rs:1028
//     → #[cfg(any(musl, ohos))]: target = max(hard限制, 163840)  resolver/lib.rs:1035-1036
//       → setrlimit 尝试同时拉高 soft 和 hard
//         → 无特权时 hard 拉不动 → EPERM → 退化为"soft 顶到当前 hard"
//
// 【为何需要】"musl/OHOS have extremely low defaults" 这句注释描述的是
// 什么数字，从未被记录过；如果默认值已经不低，这段专门的 OHOS 分支就是
// 死代码。
//
// Exit: 0=pass (默认 soft 已经 >= 163840，降级式的手工拉高不再必要)
//       1=fail (默认 soft 明显偏低，仍然需要手工拉高)

#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>

#define TARGET 163840UL

int main(void) {
    struct rlimit lim;
    if (getrlimit(RLIMIT_NOFILE, &lim) != 0) {
        fprintf(stderr, "getrlimit(RLIMIT_NOFILE): %s", strerror(errno));
        return 2;
    }

    if (lim.rlim_cur >= TARGET) {
        return 0;
    }

    fprintf(stderr, "RLIMIT_NOFILE default soft=%llu hard=%llu, below the %lu ohos-bun forces -- manual bump still needed",
            (unsigned long long)lim.rlim_cur, (unsigned long long)lim.rlim_max, TARGET);
    return 1;
}
