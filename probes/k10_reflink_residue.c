// probes/k10_reflink_residue.c
//
// 【测试目的】验证 copy_file_range() 克隆出的文件，在不先 ftruncate 清空
// 就直接覆写新内容的情况下，随后用一个全新的 fd 重新打开、读到的是不是
// 真的是新内容（而不是克隆时残留的旧内容）。
//
// 【关联项目】ohos-bun（bun build --compile 产物落盘）
//
// 【上游调用链】
// StandaloneModuleGraph 把 .bun 段写回克隆出来的可执行文件:
//   → copy_file_range() 克隆原始可执行文件到临时文件
//     → #[cfg(ohos)] ftruncate(fd, 0) 先切断 COW/reflink 关系   StandaloneModuleGraph.rs:1756
//       → write_all() 写入新内容
//         → #[cfg(ohos)] fsync() 落盘，move_file_z_with_handle
//           的 EXDEV 兜底路径靠 copy_file_range 读磁盘、不读页缓存 StandaloneModuleGraph.rs:1770
//
// 【范围说明】完整复现链条还涉及"跨设备 rename 退化为 copy_file_range"
// 这一步；本探针把问题收窄到可独立复现的核心部分——克隆后不
// ftruncate/fsync 直接覆写，是否会导致后续独立 fd 读到陈旧数据，这正是
// 两处 cfg(ohos) workaround 共同防的那类"页缓存脏了、磁盘没落盘"竞态。
//
// Exit: 0=pass (新内容能被独立 fd 正确读到 -- 两处 workaround 可能已经
//              不再必要，但仍建议先跑一次 audit-coverage.py grep 到的
//              真实 EXDEV rename 路径复核，再决定是否删除)
//       1=fail (读到旧内容或数据不一致 -- 两处 workaround 仍然必需)
//       2=unsupported (这个文件系统的 copy_file_range 本身就不支持)

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#define LEN 4096

int main(void) {
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || !*tmpdir) tmpdir = "/data/storage/el2/base/tmp";

    char src_path[512], dst_path[512];
    snprintf(src_path, sizeof(src_path), "%s/.ohos-preflight-k10-src-%d", tmpdir, getpid());
    snprintf(dst_path, sizeof(dst_path), "%s/.ohos-preflight-k10-dst-%d", tmpdir, getpid());

    char pattern_a[LEN], pattern_b[LEN];
    memset(pattern_a, 'A', LEN);
    memset(pattern_b, 'B', LEN);

    int src_fd = open(src_path, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (src_fd < 0) { fprintf(stderr, "open(src): %s", strerror(errno)); return 2; }
    if (write(src_fd, pattern_a, LEN) != LEN) {
        fprintf(stderr, "write(src): %s", strerror(errno));
        close(src_fd); unlink(src_path);
        return 2;
    }
    lseek(src_fd, 0, SEEK_SET);

    int dst_fd = open(dst_path, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (dst_fd < 0) {
        fprintf(stderr, "open(dst): %s", strerror(errno));
        close(src_fd); unlink(src_path);
        return 2;
    }

    loff_t off_in = 0, off_out = 0;
    ssize_t copied = syscall(SYS_copy_file_range, src_fd, &off_in, dst_fd, &off_out, LEN, 0);
    int cfr_errno = errno;
    close(src_fd);
    unlink(src_path);

    if (copied < 0) {
        fprintf(stderr, "copy_file_range not usable on this filesystem: %s", strerror(cfr_errno));
        close(dst_fd); unlink(dst_path);
        return 2;
    }
    if (copied != LEN) {
        fprintf(stderr, "copy_file_range copied %zd of %d bytes", copied, LEN);
        close(dst_fd); unlink(dst_path);
        return 2;
    }

    // Deliberately skip ftruncate(dst_fd, 0) and fsync() -- reproducing
    // the exact hazard the two OHOS-only workaround lines guard against.
    if (pwrite(dst_fd, pattern_b, LEN, 0) != LEN) {
        fprintf(stderr, "pwrite(dst) new content: %s", strerror(errno));
        close(dst_fd); unlink(dst_path);
        return 2;
    }
    close(dst_fd);

    // Read back through a brand new, independently-opened fd.
    int verify_fd = open(dst_path, O_RDONLY);
    if (verify_fd < 0) {
        fprintf(stderr, "reopen(dst) for verification: %s", strerror(errno));
        unlink(dst_path);
        return 2;
    }
    char readback[LEN];
    ssize_t n = read(verify_fd, readback, LEN);
    close(verify_fd);
    unlink(dst_path);

    if (n != LEN) {
        fprintf(stderr, "readback got %zd of %d bytes", n, LEN);
        return 1;
    }
    if (memcmp(readback, pattern_b, LEN) != 0) {
        int stale = (memcmp(readback, pattern_a, LEN) == 0);
        fprintf(stderr, "readback does not match freshly-written content%s",
                stale ? " -- it's the STALE copy_file_range-cloned data" : " -- unexpected garbage");
        return 1;
    }
    return 0;
}
