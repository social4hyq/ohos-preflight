// probes/i5_system_bin_sh.c
//
// 【测试目的】/system/bin/sh 是否存在且可执行。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

int main(void) {
    if (access("/system/bin/sh", X_OK) == 0) {
        return 0;
    }
    fprintf(stderr, "/system/bin/sh not executable: %s", strerror(errno));
    return 1;
}

