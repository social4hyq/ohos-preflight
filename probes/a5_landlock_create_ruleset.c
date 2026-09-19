// probes/a5_landlock_create_ruleset.c
//
// 【测试目的】Landlock LSM 规则集创建。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef SYS_landlock_create_ruleset
#define SYS_landlock_create_ruleset 444
#endif

#ifndef LANDLOCK_CREATE_RULESET_VERSION
#define LANDLOCK_CREATE_RULESET_VERSION (1U << 0)
#endif

struct landlock_ruleset_attr {
    __UINT64_TYPE__ handled_access_fs;
};

int main(void) {
    struct landlock_ruleset_attr attr = { .handled_access_fs = 0 };
    long ret = syscall(SYS_landlock_create_ruleset, &attr, sizeof(attr), 0);
    if (ret < 0) {
        perror("landlock_create_ruleset");
        return 1;
    }
    close((int)ret);
    return 0;
}

