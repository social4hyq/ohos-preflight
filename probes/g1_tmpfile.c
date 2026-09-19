// probes/g1_tmpfile.c
//
// 【测试目的】验证 tmpfile() 可用性。
// tmpfile() 创建临时文件，musl libc 内部硬编码 P_tmpdir = "/data/local/tmp"。
//
// 【上游调用链】WebKit (JSC)
//   JSC 模块缓存 / 编译产物临时文件
//     → musl tmpfile() → openat(AT_FDCWD, P_tmpdir, O_RDWR|O_CREAT|O_EXCL)
//       → P_tmpdir = "/data/local/tmp"
//         → EACCES（HarmonyOS 应用沙箱拒绝写入 /data/local/tmp）
//   WebKit 文件系统操作
//     → mkostemp(template, O_CLOEXEC)                       FileSystemPOSIX.cpp:191
//
// 【影响】OHOS 移植版 musl 将 tmpfile() 默认路径硬编码为 /data/local/tmp，
// HarmonyOS 应用沙箱拒绝写入该路径（EACCES）。依赖 tmpfile() 的标准 C 程序
// 无法创建临时文件。降级方案：mkstemp() 在应用私有目录创建（j6_tmpfile_fb）。
//
// Exit codes:
//   0 = tmpfile() succeeded, file is writable
//   1 = tmpfile() returned NULL
//   2 = (unused)

#include <stdio.h>
#include <errno.h>
#include <string.h>

int main(void) {
    FILE *fp = tmpfile();
    if (!fp) {
        fprintf(stderr, "tmpfile() failed: %s", strerror(errno));
        return 1;
    }
    if (fputs("probe", fp) == EOF) {
        fprintf(stderr, "tmpfile() succeeded but write failed: %s", strerror(errno));
        fclose(fp);
        return 1;
    }
    fclose(fp);
    return 0;
}
