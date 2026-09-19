// 替代：经典 fchmodat() 无 flags 参数，丢失 AT_SYMLINK_NOFOLLOW 语义
// 大多数 chmod 用例不需要 flags，可安全降级
#include <fcntl.h>
#include <sys/stat.h>

int chmod_compat(int dirfd, const char *path, mode_t mode) {
    // 注意：丢失 AT_SYMLINK_NOFOLLOW；如需避免改 symlink 目标
    // 应先 fstatat 检查 S_ISLNK 再决定是否调用
    return fchmodat(dirfd, path, mode, 0);
}
// TODO: 待 HarmonyOS 放行 fchmodat2 (452) 后改用
//       syscall(SYS_fchmodat2, dirfd, path, mode, flags)
