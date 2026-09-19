// probes/g8_ancestor_stat_eacces.c
//
// 【测试目的】从典型 Bun 工作路径向上 walk 各祖先目录 stat() 是否返 EACCES/EPERM。
//
// 【关联项目】Bun
//
// 【上游调用链】
// Bun resolver 向上找 node_modules / tsconfig / package.json:
//   → resolver::Resolver::dir_info_uncached                    src/resolver/resolver.rs:4398
//     → opendir/stat 每一级祖先（"/", "/storage", ...）
//       → OHOS 沙箱可能在某些祖先返 EACCES
//
// 【为何需要】fork 在 resolver.rs 加 `continue 'queue_walk` 跳过 EACCES/EPERM 祖先，
// 不视作硬错误。本探针验证从 `$HOME` / `$PWD` 向上是否真有 EACCES 祖先。
//
// 【影响】若复现：resolver 不 skip 会直接 fail；若不复现：fork 的 skip 分支可能不需要。
//
// Exit: 0=pass (祖先 walk 全部可 stat — fork EACCES skip 可能多余)
//       1=fail (某祖先 EACCES/EPERM — fork skip 必需)

#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int walk_up(const char *origin) {
    char path[PATH_MAX];
    strncpy(path, origin, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';
    int found_blocked = 0;
    while (1) {
        struct stat st;
        if (stat(path, &st) != 0) {
            if (errno == EACCES || errno == EPERM) {
                fprintf(stderr, "stat('%s') %s — ancestor blocked\n", path, strerror(errno));
                found_blocked = 1;
            } else if (errno != ENOENT) {
                fprintf(stderr, "stat('%s'): %s\n", path, strerror(errno));
            }
        }
        if (strcmp(path, "/") == 0) break;
        char *slash = strrchr(path, '/');
        if (!slash) break;
        if (slash == path) path[1] = '\0';
        else *slash = '\0';
    }
    return found_blocked;
}

int main(void) {
    const char *home = getenv("HOME");
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) {
        fprintf(stderr, "getcwd: %s", strerror(errno));
        return 1;
    }

    int blocked = 0;
    if (home && *home) blocked |= walk_up(home);
    blocked |= walk_up(cwd);
    blocked |= walk_up("/data/storage/el2/base/haps");
    blocked |= walk_up("/data/app/el1/bundle/public");

    if (blocked) {
        fprintf(stderr, "ancestor EACCES reproduces — resolver.rs queue_walk skip required");
        return 1;
    }
    return 0;
}
