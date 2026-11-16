# Pull Request

## 变更说明

<!-- 说明做了什么、为什么做、影响范围 -->

## 关联 Issue

<!-- 例如 Closes #12；没有可以留空 -->

## 自查清单

- [ ] 已运行 `make check`（数据校验 + 文档链接 + 176 项回归）并全部通过
- [ ] 修改了数据表/协议/存档格式时，同步更新了 `docs/DATA-FORMAT.md` 或 `docs/PROTOCOL.md`
- [ ] 修改了架构性代码时，同步更新了 `docs/ARCHITECTURE.md`
- [ ] 新增/修改行为时补充了 `tools/smoke_test.py` 断言（或说明不需要）
- [ ] 没有提交 `build/`、`data/players/`、`server996/Log/` 等运行时文件

## 验证记录

<!-- 贴出 make check / CI 结果，或测试步骤 -->
