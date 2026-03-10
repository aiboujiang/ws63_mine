# Copilot AI 需求完成自动归档与同步 SKILL

## 功能
- 每次需求完成后，一键执行：自动提交 + 保留前一版本 + 同步到远端。
- 远端保留 5 个滚动历史标签：
  - `copilot-backup-v1`：最新的“前一版本”。
  - `copilot-backup-v5`：最旧保留版本。
- 超过 5 个时自动覆盖最旧版本（按轮转规则前移）。

## 使用方法
在仓库根目录执行：

```bash
bash tools/copilot_skill_git_sync.sh "本次需求简述"
```

示例：

```bash
bash tools/copilot_skill_git_sync.sh "完成 slave LD2402 开关控制"
```

## 同步内容
- 当前分支（如 `main`）会推送到 `origin`。
- 轮转标签 `copilot-backup-v1` 到 `copilot-backup-v5` 会强制更新并推送到远端。

## 说明
- 如果没有文件变更，脚本会跳过提交与备份轮转，只执行分支同步。
- 如遇权限问题，请先确认 SSH key 与远端权限正常。
