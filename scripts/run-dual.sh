#!/bin/sh
# scripts/run-dual.sh
#
# 对比验证一体化脚本：本机 harmonyos → 容器 openharmony → 对比 → 保存报告。
# 报告输出到 stdout 同时保存到 reports/preflight-matrix-YYYY-MM-DD-HHMM.html。
#
# Usage:
#   ./scripts/run-dual.sh
#   ./scripts/run-dual.sh | tee /tmp/latest-matrix.html
#
# 前置条件：
#   容器已完成一次性配置（参见 README.md）

set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
TMPDIR="${TMPDIR:-$HOME/.tmp}"
CONTAINER="${CONTAINER:-openharmony}"
REMOTE_DIR="${REMOTE_DIR:-/root/ohos-preflight}"
CONTAINER_NDK="${CONTAINER_NDK:-/opt/ohos-sdk/ohos/native}"
OHOS_NDK_HOME="${OHOS_NDK_HOME:-$(ls -d "$HOME"/.harmonybrew/Cellar/ohos-sdk/*/native 2>/dev/null | sort -V | tail -1)}"
export CONTAINER REMOTE_DIR CONTAINER_NDK
REPORTS_DIR="$PROJECT_DIR/reports"
NOW="$(date +%Y-%m-%d-%H%M)"

HM_FILE="$TMPDIR/harmonyos.jsonl"
CI_FILE="$TMPDIR/openharmony.jsonl"
HM_CLEAN="$TMPDIR/harmonyos_clean.jsonl"
CI_CLEAN="$TMPDIR/openharmony_clean.jsonl"
META_FILE="$TMPDIR/preflight-meta.json"
REPORT_FILE="$REPORTS_DIR/preflight-matrix-$NOW.html"

mkdir -p "$REPORTS_DIR"

echo "=== 对比验证 $NOW ===" >&2
echo "" >&2

# 1. 本机 harmonyos 轨道
echo "[1/5] 本机 HarmonyOS 探针编译与运行..." >&2
cd "$PROJECT_DIR" || exit 1
make clean >/dev/null 2>&1
if ! make >/dev/null 2>&1; then
    echo "ERROR: 本机编译失败" >&2
    exit 1
fi
TRACK=harmonyos ./run.sh > "$HM_FILE" 2>&1
grep '{"probe"' "$HM_FILE" > "$HM_CLEAN"
echo "      本机完成: $(wc -l < "$HM_CLEAN") 条探针" >&2

# 2. 容器 openharmony 轨道（先同步源码，再编译运行）
echo "[2/5] 容器 OpenHarmony 探针编译与运行..." >&2
tar czf "$TMPDIR/ohos-preflight.tar.gz" --exclude='.git' --exclude='reports' --exclude='__pycache__' -C "$PROJECT_DIR" . 2>/dev/null
docker cp "$TMPDIR/ohos-preflight.tar.gz" "$CONTAINER:/tmp/" 2>/dev/null
docker exec "$CONTAINER" bash -c "rm -rf $REMOTE_DIR && mkdir -p $REMOTE_DIR && cd $REMOTE_DIR && tar xzf /tmp/ohos-preflight.tar.gz" 2>/dev/null
"$SCRIPT_DIR/run-container.sh" > "$CI_FILE" 2>&1
grep '{"probe"' "$CI_FILE" > "$CI_CLEAN"
echo "      容器完成: $(wc -l < "$CI_CLEAN") 条探针" >&2

# 3. 收集系统版本信息（make 已编译 _os_version，直接运行）
echo "[3/5] 收集系统版本信息..." >&2
HM_VER="$(cd "$PROJECT_DIR" && ./probes/_os_version 2>/dev/null)"
CI_VER="$(docker exec "$CONTAINER" "$REMOTE_DIR/probes/_os_version" 2>/dev/null)"
HM_CLANG="$("$OHOS_NDK_HOME/llvm/bin/clang" --version 2>/dev/null | head -1)"
CI_CLANG="$(docker exec "$CONTAINER" bash -lc "$CONTAINER_NDK/llvm/bin/clang --version 2>/dev/null | head -1" 2>/dev/null)"

# HarmonyOS 侧用 param get 获取详细版本（比 uname 更精确）
HM_OS_NAME="$(param get const.product.os.dist.name 2>/dev/null || echo 'HarmonyOS')"
HM_OS_VER="$(param get const.product.os.dist.version 2>/dev/null)"
HM_OHOS_FULL="$(param get const.ohos.fullname 2>/dev/null)"
HM_OHOS_CERT="$(param get const.ohos.version.certified 2>/dev/null)"
HM_BUILD="$(param get const.product.software.version 2>/dev/null)"
HM_HARDWARE="$(param get ohos.boot.hardware 2>/dev/null)"

# 容器的 param 不可用，保留 _os_version 探针的结果
CI_KERNEL_VER="$(echo "$CI_VER" | python3 -c "import json,sys; d=json.load(sys.stdin); print(d.get('release',''))" 2>/dev/null)"
CI_API="$(echo "$CI_VER" | python3 -c "import json,sys; d=json.load(sys.stdin); print(d.get('oh_current_api',''))" 2>/dev/null)"

python3 -c "
import json
meta = {
    'hm': {
        'label': '''$HM_OS_NAME''' + (' ' + '''$HM_OS_VER''' if '''$HM_OS_VER''' else ''),
        'kernel': 'HongMeng Kernel ' + json.loads('''$HM_VER''').get('release','').split()[-1] if json.loads('''$HM_VER''').get('release','') else '',
        'ohos': '''$HM_OHOS_CERT''' or '''$HM_OHOS_FULL''' or '',
        'build': '''$HM_BUILD''',
        'hardware': '''$HM_HARDWARE''',
        'clang': '''$HM_CLANG''',
    },
    'ci': {
        'label': 'OpenHarmony v6.1-Release (API ' + ('''$CI_API''' or '26') + ', 无沙箱)',
        'kernel': 'Linux ' + ('''$CI_KERNEL_VER''' or '6.6.0') + ' (Docker 宿主机内核)',
        'ohos': 'OpenHarmony v6.1-Release (rk3568) — 已移除 init/SELinux，无应用沙箱',
        'build': 'FROM scratch, dockerharmony 项目构建',
        'hardware': 'Docker 容器 (ARM64)',
        'clang': '''$CI_CLANG''',
    }
}
print(json.dumps(meta, ensure_ascii=False))
" > "$META_FILE" 2>/dev/null
echo "      版本信息已收集" >&2

# 4. 生成对比报告（HTML + Markdown + JSON 三种格式）
echo "[4/5] 生成对比报告..." >&2
MD_FILE="$REPORTS_DIR/preflight-summary-$NOW.md"
JSON_FILE="$REPORTS_DIR/preflight-data-$NOW.json"
python3 "$PROJECT_DIR/html.py" "$CI_CLEAN" "$HM_CLEAN" -m "$META_FILE" -o "$REPORT_FILE"
python3 "$PROJECT_DIR/markdown.py" "$CI_CLEAN" "$HM_CLEAN" -m "$META_FILE" -o "$MD_FILE"
python3 "$PROJECT_DIR/json_report.py" "$CI_CLEAN" "$HM_CLEAN" -m "$META_FILE" -o "$JSON_FILE"
echo "      HTML: $REPORT_FILE" >&2
echo "      MD:   $MD_FILE" >&2
echo "      JSON: $JSON_FILE" >&2

# 5. 输出到 stdout
echo "[5/5] 完成" >&2
cat "$REPORT_FILE"
