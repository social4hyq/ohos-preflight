// probes/j13_seccomp_shim_fb.c
//
// 【测试目的】a1_seccomp_unotify 替代：LD_PRELOAD shim 拦截 libc open()。
//   验证两个条件，缺一不可：
//   1. 拦截发生 — shim 把被打开的路径写进了 $J13_LOG
//   2. 转发正确 — 经 RTLD_NEXT 转发后仍能读回文件内容
//
// 【关联项目】vite-plus（fspy 系统调用审计）
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAGIC "j13-shim-roundtrip"

static void make_path(char *buf, size_t n, const char *base, const char *name) {
    snprintf(buf, n, "%s/.tmp/%s.%d", base, name, (int)getpid());
}

int main(int argc, char **argv) {
    const char *base = getenv("HOME");
    if (!base) base = "/tmp";
    char dir[256], target[320], logf[320];
    snprintf(dir, sizeof(dir), "%s/.tmp", base);
    mkdir(dir, 0755);
    make_path(target, sizeof(target), base, "j13_target");
    make_path(logf, sizeof(logf), base, "j13_log");

    if (getenv("J13_PASS") == NULL) {
        // First pass: drop the roundtrip file, re-exec with the shim injected.
        FILE *f = fopen(target, "w");
        if (!f) { perror("fopen target"); return 2; }
        fputs(MAGIC, f);
        fclose(f);
        unlink(logf);
        setenv("LD_PRELOAD", "./probes/_j13shim.so", 1);
        setenv("J13_LOG", logf, 1);
        setenv("J13_PASS", "1", 1);
        execv(argv[0], argv);
        perror("execv");
        return 2;
    }

    // Second pass (shim preloaded): open() must be both intercepted and
    // forwarded.
    int rc = 1;
    char buf[64] = {0};
    int fd = open(target, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "shim forward failed: open() returned errno\n");
        goto out;
    }
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n != (ssize_t)strlen(MAGIC) || strcmp(buf, MAGIC) != 0) {
        fprintf(stderr, "shim forward corrupted: read '%s'\n", buf);
        goto out;
    }

    FILE *lf = fopen(logf, "r");
    if (!lf) {
        fprintf(stderr, "shim did not intercept open() (no log %s — LD_PRELOAD likely blocked)\n", logf);
        goto out;
    }
    int intercepted = 0;
    char line[512];
    while (fgets(line, sizeof(line), lf)) {
        line[strcspn(line, "\n")] = 0;
        if (strcmp(line, target) == 0) { intercepted = 1; break; }
    }
    fclose(lf);
    if (!intercepted) {
        fprintf(stderr, "shim log exists but target path missing\n");
        goto out;
    }
    rc = 0;

out:
    unlink(target);
    unlink(logf);
    return rc;
}
