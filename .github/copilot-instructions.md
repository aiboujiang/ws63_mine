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
