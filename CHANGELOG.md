# 更新日志（CHANGELOG）

本文件记录项目的可见变更。格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循语义化版本（主版本.次版本.修订号）。

## [Unreleased]

## [v0.1.1] - 2026-11-16

维护性收尾：把仓库从"能跑 + 有 CI"补到"可持续维护"。

### 新增

- `CHANGELOG.md`（本文件）、`.github/ISSUE_TEMPLATE/` 与 PR 模板
- `make check`：数据校验 + 文档链接检查 + 176 项回归的一键入口
- `tools/check_links.py`：检查 README/CHANGELOG/NOTICE/docs 中的相对链接，CI 中运行
- `tools/package_release.py` + `make package`：生成可直接运行的 server 发布包（结构包含 `build/GameServer`、`data/`、`tools/`、`gameserver.ini`，解包即可运行回归）
- Release 附件：`woool2005-v0.1.1-macos-arm64.tar.gz`

### 变更

- CI 的构建任务新增"文档链接检查"步骤（Ubuntu / macOS 都会跑）
- README 增加 CHANGELOG、`make check` 与 issue 模板入口

## [v0.1.0] - 2026-11-16

首个带工程保障的版本（代码基线为 2005 年服务端快照重建 + 玩法扩展）。

### 新增

- GitHub Actions CI：Ubuntu / macOS 构建 + 数据表校验 + 176 项端到端回归；Ubuntu 独立 ASan + UBSan 全量回归
- `tools/validate_data.py`：20 张数据表、2350 项静态校验（字段数 / 数值范围 / 交叉引用 / 地图完整性），支持 `--only` / `--strict` / `--quiet`
- 结构化文档：`docs/ARCHITECTURE.md`（454 行）、`docs/PROTOCOL.md`（428 行）、`docs/DATA-FORMAT.md`（327 行），关键结论带源码行号
- `NOTICE.md`：2005 原始代码、重建代码、新增代码的来源与授权状态说明
- `tools/capture_screenshots.py` + 5 张无头截图（登录 / 选角 / 世界 / 背包 / 副本），README 展示
- Makefile：`make validate`、`make screenshots`

### 验证

- 本地 macOS clang++ 构建 0 error；`make smoke` 176/176 通过；ASan + UBSan 下 176/176 通过
- CI 全绿（`main` @ `ca0f74d`）；数据校验 2350 项 0 error / 16 warning

### 已知问题

- 16 条校验 warning：`ITEMBASE.szName[16]` 截断 17–20 字节中文装备名，按名查找/批量卖出会失效
- 交易原子性、换装回滚、物品实例号持久化、技能位图 32 位、副本超时清理等数据安全类问题待修
- 协议为重建实现，不能直连真实 2005 客户端；SC/DB 仍为桩

## [v0.0.0] - 2026-11-16

初始导入（`a0f12cb`）：2005 年原始快照 40 个文件、重建引擎层、玩法扩展、客户端、工具链与 996 外壳。
