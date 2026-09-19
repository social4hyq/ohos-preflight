# scripts/ohos-preflight/Makefile
#
# Cross-compiles the preflight probe matrix using the OHOS NDK clang.
# Default NDK path is HarmonyOS-local; override OHOS_NDK_HOME to retarget.
#
# Targets:
#   make            -> build every C probe + chmod +x shell wrappers
#   make clean      -> remove built C probes
#   make run        -> build then invoke ./run.sh

# Default: newest harmonybrew-installed ohos-sdk under $HOME. Override to retarget.
OHOS_NDK_HOME ?= $(shell ls -d $(HOME)/.harmonybrew/Cellar/ohos-sdk/*/native 2>/dev/null | sort -V | tail -1)
CLANG := $(OHOS_NDK_HOME)/llvm/bin/clang
SYSROOT := $(OHOS_NDK_HOME)/sysroot
CC = $(CLANG) --target=aarch64-linux-ohos --sysroot=$(SYSROOT)
CFLAGS = -O0 -g -Wall
LDFLAGS = -ldl

PROBES_C := $(wildcard probes/*.c)
# Hook/shim sources are built as shared libraries, not standalone binaries.
PROBES_C_BIN := $(filter-out probes/b1_hook probes/j13_shim,$(PROBES_C:.c=))

PROBES_SH := $(wildcard probes/*.sh)

SHARED_LIBS := probes/libb1hook.so probes/_j13shim.so

.PHONY: all clean run shell-chmod

all: $(PROBES_C_BIN) $(SHARED_LIBS) shell-chmod

# Build any single-file C probe.
probes/%: probes/%.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

# B1 preload hook -> shared library (constructor writes a sentinel tmpfile).
probes/libb1hook.so: probes/b1_hook.c
	$(CC) $(CFLAGS) -shared -fPIC $< -o $@

# J13 open() interposer shim. Underscore prefix keeps run.sh from executing it.
probes/_j13shim.so: probes/j13_shim.c
	$(CC) $(CFLAGS) -shared -fPIC $< -o $@ -ldl

# Mark every shell-script probe executable.
shell-chmod:
	@for s in $(PROBES_SH); do chmod +x $$s; done

clean:
	rm -f $(PROBES_C_BIN) $(SHARED_LIBS)

run: all
	./run.sh
