// 替代：openat() + O_NOFOLLOW + fstatat 路径段扫描模拟 RESOLVE_NO_SYMLINKS
// 注：仍有 TOCTOU 窗口，安全性弱于 openat2 的原子语义
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int openat_no_symlinks(int dirfd, const char *path, int flags) {
    char buf[PATH_MAX];
    if (strlen(path) >= sizeof buf) { errno = ENAMETOOLONG; return -1; }
    strcpy(buf, path);

    // 校验路径中间每一段都不是 symlink
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
    // 末段用 O_NOFOLLOW 防 symlink
    return openat(dirfd, path, flags | O_NOFOLLOW);
}
// TODO: 待 HarmonyOS 放行 openat2 (437) 后改用
//   syscall(SYS_openat2, dirfd, path, &(struct open_how){.flags=...,
//     .resolve=RESOLVE_NO_SYMLINKS}, sizeof(struct open_how))
