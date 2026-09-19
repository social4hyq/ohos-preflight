// probes/k11_dlopen_unsigned_so.c
//
// 【测试目的】验证内核是否仍然拒绝 dlopen() 一个 .codesign 段内容已经
// 失效（未签名/签名被破坏）的 .so。
//
// 【关联项目】ohos-bun（npm 原生依赖安装）
//
// 【上游调用链】
// bun install 装完带 .so/.node 的包之后:
//   → ohos_sign_native_binaries(root_dir)                    PackageInstaller.rs:2537
//     → 递归扫描 root_dir 下的 .so/.node
//       → ohos_sign::has_codesign() 检查是否已签名
//         → 未签名则 ohos_sign::sign_selfsign_inplace() 补签
//           → "OHOS kernel refuses to dlopen unsigned .so"  PackageInstaller.rs:2524
//
// 【为何需要】这条安装期强制签名的注释断言过内核行为，但从未有独立探针
// 验证过——本探针直接构造一个 .codesign 段内容被清零（等价于签名失效）
// 的 .so 副本，测 dlopen() 到底会不会被拒。
//
// 手法：拿一个磁盘上已经存在、体积很小的真实 .so（ohos-compat-shim 的
// libohos_compat.so，随 formula 一起装的固定路径），复制一份出来，用
// ELF64 section header 定位并清零它的 .codesign 段内容,再 dlopen 这份
// 被破坏的副本。
//
// Exit: 0=pass (dlopen 一个签名已失效的 .so 仍然成功 -- PackageInstaller.rs
//              的安装期强制签名扫描可能已经不再必要)
//       1=fail (dlopen 被拒 -- 强制签名扫描仍然必需)
//       2=unsupported (找不到参考 .so，或它没有 .codesign 段可破坏)

#define _GNU_SOURCE
#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *candidate_paths[] = {
    NULL, // filled in with $HOME-based path at runtime
    "/opt/ohos-compat-shim/lib/libohos_compat.so",
};

static int copy_file(const char *src, const char *dst) {
    int in = open(src, O_RDONLY);
    if (in < 0) return -1;
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if (out < 0) { close(in); return -1; }
    char buf[65536];
    ssize_t n;
    int ok = 1;
    while ((n = read(in, buf, sizeof(buf))) > 0) {
        if (write(out, buf, (size_t)n) != n) { ok = 0; break; }
    }
    if (n < 0) ok = 0;
    close(in);
    close(out);
    return ok ? 0 : -1;
}

// Zero out the named section's on-disk bytes in-place, invalidating any
// hash/signature embedded there. Returns 0 on success, -1 if the section
// isn't found or the file isn't a well-formed ELF64.
static int zero_section(const char *path, const char *section_name) {
    int fd = open(path, O_RDWR);
    if (fd < 0) return -1;

    Elf64_Ehdr eh;
    if (read(fd, &eh, sizeof(eh)) != (ssize_t)sizeof(eh) ||
        memcmp(eh.e_ident, ELFMAG, SELFMAG) != 0 || eh.e_ident[EI_CLASS] != ELFCLASS64) {
        close(fd);
        return -1;
    }
    if (eh.e_shoff == 0 || eh.e_shnum == 0 || eh.e_shstrndx == SHN_UNDEF) {
        close(fd);
        return -1;
    }

    Elf64_Shdr shstrtab_hdr;
    if (pread(fd, &shstrtab_hdr, sizeof(shstrtab_hdr),
              (off_t)(eh.e_shoff + (Elf64_Off)eh.e_shstrndx * sizeof(Elf64_Shdr))) != (ssize_t)sizeof(shstrtab_hdr)) {
        close(fd);
        return -1;
    }
    char *shstrtab = malloc(shstrtab_hdr.sh_size);
    if (!shstrtab || pread(fd, shstrtab, shstrtab_hdr.sh_size, (off_t)shstrtab_hdr.sh_offset) != (ssize_t)shstrtab_hdr.sh_size) {
        free(shstrtab);
        close(fd);
        return -1;
    }

    int found = -1;
    for (int i = 0; i < eh.e_shnum; i++) {
        Elf64_Shdr sh;
        if (pread(fd, &sh, sizeof(sh), (off_t)(eh.e_shoff + (Elf64_Off)i * sizeof(Elf64_Shdr))) != (ssize_t)sizeof(sh))
            continue;
        if (sh.sh_name >= shstrtab_hdr.sh_size) continue;
        if (strcmp(shstrtab + sh.sh_name, section_name) != 0) continue;

        char *zeros = calloc(1, sh.sh_size);
        if (zeros && pwrite(fd, zeros, sh.sh_size, (off_t)sh.sh_offset) == (ssize_t)sh.sh_size) {
            found = 0;
        }
        free(zeros);
        break;
    }
    free(shstrtab);
    close(fd);
    return found;
}

int main(void) {
    char home_candidate[512];
    const char *home = getenv("HOME");
    if (home && *home) {
        snprintf(home_candidate, sizeof(home_candidate), "%s/.harmonybrew/opt/ohos-compat-shim/lib/libohos_compat.so", home);
        candidate_paths[0] = home_candidate;
    }

    const char *src = NULL;
    for (size_t i = 0; i < sizeof(candidate_paths) / sizeof(candidate_paths[0]); i++) {
        if (candidate_paths[i] && access(candidate_paths[i], R_OK) == 0) {
            src = candidate_paths[i];
            break;
        }
    }
    if (!src) {
        fprintf(stderr, "no reference .so found (expected ohos-compat-shim installed via brew)");
        return 2;
    }

    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || !*tmpdir) tmpdir = "/data/storage/el2/base/tmp";
    char copy_path[512];
    snprintf(copy_path, sizeof(copy_path), "%s/.ohos-preflight-k11-%d.so", tmpdir, getpid());

    if (copy_file(src, copy_path) != 0) {
        fprintf(stderr, "failed to stage a copy at %s: %s", copy_path, strerror(errno));
        return 2;
    }
    if (zero_section(copy_path, ".codesign") != 0) {
        fprintf(stderr, "reference .so has no .codesign section to invalidate (or isn't ELF64)");
        unlink(copy_path);
        return 2;
    }

    void *handle = dlopen(copy_path, RTLD_LAZY | RTLD_LOCAL);
    unlink(copy_path);

    if (!handle) {
        fprintf(stderr, "dlopen refused .so with invalidated .codesign: %s", dlerror());
        return 1;
    }
    dlclose(handle);
    return 0;
}
