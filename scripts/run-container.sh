#!/bin/sh
# scripts/run-container.sh
#
# Run all probes inside the openharmony container and emit JSONL to stdout.
# Handles env setup so the caller doesn't need to remember NDK paths,
# brew cargo PATH, or the /system/bin/sh workaround.
#
# Usage:
#   ./scripts/run-container.sh > openharmony.jsonl
#   ./scripts/run-container.sh | grep '"probe"'
#
# One-time container setup (run once beforehand): see README.md.
# Requires local docker CLI with ohos-builder context (见仓库 README 的容器配置说明).
#
# Env overrides:
#   CONTAINER      container name                   (default openharmony)
#   REMOTE_DIR     project dir inside the container (default /root/ohos-preflight)
#   CONTAINER_NDK  NDK path inside the container    (default /opt/ohos-sdk/ohos/native)

set -u

CONTAINER="${CONTAINER:-openharmony}"
REMOTE_DIR="${REMOTE_DIR:-/root/ohos-preflight}"
CONTAINER_NDK="${CONTAINER_NDK:-/opt/ohos-sdk/ohos/native}"

docker exec "$CONTAINER" bash -lc "
export OHOS_NDK_HOME=$CONTAINER_NDK
cd $REMOTE_DIR || exit 1
make clean && make && TRACK=openharmony ./run.sh
"
