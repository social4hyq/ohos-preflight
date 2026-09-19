# ohos-preflight

OpenHarmony / HarmonyOS 系统能力探针框架：交叉编译一组最小 C 程序，在目标平台上逐项探测
libc、系统调用、权限与文件系统语义，产出可对比的报告。

本仓库是**精简发布版**，只保留探针源码、运行脚本、报告生成器与本轮真机报告。

## 本轮报告

`reports/preflight-*-2026-09-19-1905.*`（JSON / Markdown / HTML 三种格式）

| 项 | 值 |
|---|---|
| 真机 | HarmonyOS 7.0.0（OpenHarmony 7.0 Release，build `HAD-W24 7.0.0.105`，KirinX90） |
| 容器 | OpenHarmony v6.1-Release（无沙箱，代表用户态能力上限） |
| 探针执行 | 本机 `TRACK=harmonyos ./run.sh` 输出 **111 条**结果：**81 pass / 30 fail** |
| 对比报告 | 「探针总数 **75**」——只统计两端可比对的项 |

> ⚠️ **两种计数口径不同**：`run.sh` 为每个可执行探针输出一条 JSONL 结果（111 条）；
> 对比报告会把只在单端有效或无法比对的项排除，因此报告里的「探针总数」小于 111。
> 引用时请说明用的是哪个口径。

## 运行

依赖 OHOS NDK（默认取 `~/.harmonybrew/Cellar/ohos-sdk/*/native`，可用 `OHOS_NDK_HOME` 覆盖）。

```bash
# 本机（HarmonyOS 真机）
make clean && make && TRACK=harmonyos ./run.sh > harmonyos.jsonl

# 双轨对比（本机 + OpenHarmony 容器），生成三种格式报告到 reports/
./scripts/run-dual.sh
```

`run.sh` 为每个探针输出一行 JSON：`{"probe":…,"track":…,"status":"pass|fail|unsupported","reason":…}`。
退出码 `0` = pass，`1` = fail（能力不可用），`2` = unsupported（前置条件不满足）。

容器轨的一次性配置（容器重建后需重跑）：

```bash
docker exec openharmony bash -lc "mkdir -p /system/bin && ln -sf /bin/sh /system/bin/sh && mkdir -p /data/local/tmp"
```

## 目录

| 路径 | 内容 |
|---|---|
| `probes/` | 探针源码（`.c` / `.sh`）与元数据的实现 |
| `probes.toml` | 探针元数据：分类、说明、状态标签、失败原因归类 |
| `solutions/` | 各探针的替代方案代码（报告里「建议」一节的来源） |
| `run.sh` | 遍历并执行探针，输出 JSONL |
| `Makefile` | 交叉编译入口 |
| `html.py` / `markdown.py` / `json_report.py` | 报告生成器（输入两端 JSONL，输出三种格式） |
| `scripts/` | 容器轨与双轨编排脚本 |
| `smoke/` `tests/` | 框架自检 |

## 说明

- 探针按能力域分组（`a*` 系统调用与安全、`c*` 内核语义、`i*` 信息获取等），
  完整分类见 `probes.toml` 与报告正文。
- 容器轨没有应用沙箱与 SELinux，代表用户态理论能力上限；真机轨才是实际可用性的判据。
- 「需申请放行」指容器通过、真机受限的项——这些是向平台侧提出的能力诉求。
