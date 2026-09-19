// probes/b1_hook.c
//
// Built into libb1hook.so. Its only job is to drop a sentinel tmpfile via a
// constructor — the b1_ld_preload probe then reads the file back to confirm
// LD_PRELOAD actually loaded the library before the child binary's main().
//
// Sentinel path is fixed and contains the loader's PID so concurrent runs
// don't collide.

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

__attribute__((constructor))
static void b1_marker(void) {
    // HarmonyOS /tmp is read-only; fall back to ~/.tmp which is always writable.
    const char *base = getenv("HOME");
    if (!base) base = "/tmp";
    char dir[256], path[320];
    snprintf(dir, sizeof(dir), "%s/.tmp", base);
    mkdir(dir, 0755);
    snprintf(path, sizeof(path), "%s/b1_hook.%d", dir, (int)getpid());
    FILE *f = fopen(path, "w");
    if (f) {
        fputs("loaded\n", f);
        fclose(f);
    }
}

int marker(void) { return 42; }
