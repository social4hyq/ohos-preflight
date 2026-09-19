#include <pthread.h>
#include <signal.h>

volatile int cancelled = 0;
void on_cancel(int sig) { cancelled = 1; }

void *worker(void *arg) {
    signal(SIGUSR1, on_cancel);
    while (!cancelled) { }
    pthread_exit(NULL);
}
// pthread_cancel() 是空桩 → 改用 pthread_kill + 信号
pthread_kill(tid, SIGUSR1);
pthread_join(tid, NULL);
