# Copilot Requirement Completion Skill

When a requirement is completed, always run this command before closing the task:

```bash
bash tools/copilot_skill_git_sync.sh "short requirement summary"
```

Rules:
- The command must run after code changes are done and validated.
- The script creates one completion commit automatically when there are file changes.
- The script preserves the previous code version to rotating git tags:
  - `copilot-backup-v1` is the newest previous version.
  - `copilot-backup-v5` is the oldest retained version.
  - When there are more than 5 versions, older ones are overwritten from oldest to newest by rotation.
- The script always syncs the current branch to remote `origin`.
- The script force-updates rotating backup tags on remote.

If sync fails (network or auth), report the failure and retry after the issue is fixed.

## Markdown Maintenance Rule (Mandatory)

Use skill `task-md-maintenance` at the end of every completed task unless user explicitly says to skip doc maintenance.

Required behavior before task closure:
1. Update at least one relevant `.md` file.
2. Include: change summary, affected files, verification command/result, and risk/follow-up (if any).
3. For ws63_final related tasks, prefer maintaining `src/application/mine/ws63_final/README.md` under section `任务维护记录`.

Skill location in this repository:
- `.github/skills/task-md-maintenance/SKILL.md`
- `.github/skills/mandatory-menuconfig-check/SKILL.md`

## Build Precheck Rule (Mandatory)

Before running any `python3 build.py ...` command, always execute:

```bash
bash tools/check_ws63_menuconfig.sh
```

If the check fails, stop build immediately, fix menuconfig first, then rerun the check until it passes.
