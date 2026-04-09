---
name: task-md-maintenance
description: Markdown maintenance enforcement skill. MUST use this skill at the end of every completed task (code edit, config update, refactor, bugfix, doc update, or review) to update at least one relevant .md file with latest change summary, affected files, and verification result. Trigger on words like 维护.md, 更新文档, README, 记录变更, 文档同步, 任务收尾, and also as default post-task behavior unless user explicitly disables it.
---

# Task Markdown Maintenance

Use this skill to ensure every completed task leaves an auditable Markdown trail.

## Trigger Rules

Run this skill when any of the following is true:
1. User asks to maintain/update Markdown or README.
2. Task includes code/config/script/doc changes.
3. Task is completed and user did not explicitly disable doc maintenance.

## Target File Selection

Select markdown target by priority:
1. If task touches ws63_final module, update `fbb_ws63_20260114/src/application/mine/ws63_final/README.md`.
2. Otherwise update the nearest module README in changed directory.
3. If no nearby README exists, update `fbb_ws63_20260114/README.md`.

## Required Update Content

Each maintenance entry must include:
1. Date and short title.
2. What changed (2-6 concise bullets).
3. Affected file list.
4. Build/test/verification command and result.
5. Risks or follow-up (if any).

## Format Requirements

1. Prefer appending under section `任务维护记录` or `变更记录`.
2. If section does not exist, create it once and append entries chronologically.
3. Keep entries concise and factual; avoid long prose.

## Completion Gate

Before declaring task finished, check:
1. At least one relevant `.md` file updated.
2. Entry includes verification status.
3. Response mentions which `.md` file was maintained.

If user explicitly says “本次不要维护文档”, you may skip and state skip reason.
