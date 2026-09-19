// probes/i4_data_local_tmp.c
//
// 【测试目的】/data/local/tmp 临时目录是否可写。
//
// 【关联项目】通用
//
// 【上游调用链】
// ADB push / 开发脚本 / CI 部署
//   → write("/data/local/tmp/<file>")
//     → EACCES（HarmonyOS 应用沙箱拒绝该路径写入）
//
// 【影响】/data/local/tmp 是 ADB / 开发者工作流的常用临时目录，HarmonyOS 应用沙箱拒绝该路径写入。影响调试工具上传、shell 脚本中转和开发期文件分发。
//
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

int main(void) {
    if (access("/data/local/tmp", W_OK) == 0) {
        return 0;
    }
    fprintf(stderr, "/data/local/tmp not writable: %s", strerror(errno));
    return 1;
}

