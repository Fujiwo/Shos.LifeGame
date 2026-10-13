---
name: markdown-quality-audit
description: "Audit markdown quality for this documentation repository. Use when checking broken links, unclosed code fences, oversized code blocks, table-of-contents consistency, backlink consistency, or pre-publish issues in 資料/website/MarkDown or manuscript markdown."
argument-hint: "Target scope and focus, for example: 資料/website/MarkDown, or a single markdown file"
user-invocable: true
disable-model-invocation: false
---

# Markdown Quality Audit

Example invocation: Audit 資料/website/MarkDown for broken links, code fence issues, and table-of-contents consistency.

## Purpose

Run repeatable quality checks for markdown-heavy content in this repository and summarize the result in a practical review format.

This skill is designed for a documentation-first repository that manages:
- Japanese and English book manuscripts
- review documents
- static website markdown
- publication preparation materials

## When to Use

Use this skill when you need to:
- find broken markdown links
- detect unclosed code fences
- inspect oversized code fences
- verify table-of-contents and chapter title consistency
- verify chapter backlink consistency
- do a pre-publish markdown audit before review or release

Typical trigger phrases include:
- markdown quality check
- pre-publish markdown audit
- broken links in markdown
- code fence problems
- table of contents consistency
- backlink consistency
- website markdown review

Do not use this skill for:
- rewriting chapter content from scratch
- deep editorial review of argument quality
- Japanese and English translation quality comparison beyond markdown structure

## Inputs

Expected inputs may include:
- target directory
- target file
- whether the run should fail on findings
- whether the output should be concise or detailed

Examples:
- Audit 資料/website/MarkDown before publishing
- Review a single chapter file for navigation and link problems

## Procedure

1. Identify the target scope.
   - Prefer a directory for pre-publish checks.
   - Prefer a single file only when reviewing one chapter or appendix.

2. Review the checklist in [references/checklist.md](./references/checklist.md).

3. Choose the execution path.
   - On Windows, prefer [scripts/run-quality-audit.cmd](./scripts/run-quality-audit.cmd).
   - If needed, the PowerShell implementation is [scripts/run-quality-audit.ps1](./scripts/run-quality-audit.ps1).

4. Run the repository checks.
   - ../../tools/check_markdown_links.py
   - ../../tools/check_unclosed_code_fences.py
   - ../../tools/check_code_fences_dir.py
   - ../../tools/review_slim_markdown.py

5. Group findings by severity.
   - Blocking: broken links, missing linked files, malformed code fences
   - Important: heading and backlink inconsistency
   - Minor: cleanup candidates that do not block publication

6. Summarize the result.
   - What failed
   - Where it failed
   - What to fix first
   - Whether the content is ready for the next review step

## Output Format

Return:
- a short conclusion
- a flat list of findings ordered by severity
- concrete next actions

## Repository Notes

This repository is documentation-first, not application-first.
Do not assume application build pipelines or unit tests are the primary validation path.
Prefer markdown integrity, navigation integrity, and static-site content quality checks.

On Windows, some environments block direct .ps1 execution by policy.
In that case, use the .cmd wrapper, which invokes PowerShell with a process-scoped execution policy bypass for this audit only.