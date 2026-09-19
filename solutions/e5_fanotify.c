#include <sys/inotify.h>

int fd = inotify_init1(IN_NONBLOCK);
int wd = inotify_add_watch(fd, path, IN_MODIFY);
// fanotify 被沙箱限制 → 改用 inotify
struct inotify_event ev;
ssize_t n = read(fd, &ev, sizeof(ev));
