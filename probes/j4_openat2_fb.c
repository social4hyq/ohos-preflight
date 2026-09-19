// probes/j4_openat2_fb.c
//
// 【测试目的】c4_openat2 替代：openat + O_NOFOLLOW + fstatat 路径段扫描。
//
// 【关联项目】通用
//
// 【上游调用链】
//   (参见 probes.toml trigger_path)
//
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int openat_no_symlinks(int dirfd, const char *path, int flags) {
    char buf[PATH_MAX];
    if (strlen(path) >= sizeof buf) { errno = ENAMETOOLONG; return -1; }
    strcpy(buf, path);

    for (char *p = buf + 1; (p = strchr(p, '/')); p++) {
        *p = '\0';
        struct stat st;
        if (fstatat(dirfd, buf, &st, AT_SYMLINK_NOFOLLOW) == 0
            && S_ISLNK(st.st_mode)) {
            errno = ELOOP;
            return -1;
        }
        *p = '/';
    }
    return openat(dirfd, path, flags | O_NOFOLLOW);
}

int main(void) {
    const char *dir = getenv("HOME");
    if (!dir || !*dir) dir = "/tmp";
    char real_path[256], link_path[256];
    snprintf(real_path, sizeof real_path, "%s/j4_real_%d", dir, getpid());
    snprintf(link_path, sizeof link_path, "%s/j4_link_%d", dir, getpid());

    int fd = open(real_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        fprintf(stderr, "create real failed in %s: %m", dir);
        return 1;
    }
    close(fd);

    if (symlink(real_path, link_path) < 0) {
        unlink(real_path);
        fprintf(stderr, "symlink failed: %m");
        return 1;
    }

    int f_real = openat_no_symlinks(AT_FDCWD, real_path, O_RDONLY);
    int real_ok = (f_real >= 0);
    if (f_real >= 0) close(f_real);

    int f_link = openat_no_symlinks(AT_FDCWD, link_path, O_RDONLY);
    int link_blocked = (f_link < 0 && errno == ELOOP);
    if (f_link >= 0) close(f_link);

    unlink(link_path);
    unlink(real_path);

    if (real_ok && link_blocked) return 0;
    fprintf(stderr, "real_ok=%d link_blocked=%d errno=%d",
            real_ok, link_blocked, errno);
    return 1;
}

