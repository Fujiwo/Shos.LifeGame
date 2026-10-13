# Markdown Quality Audit Checklist

Use this checklist before or after running the automated checks.

## Core Checks

- Confirm the target scope is correct.
- Confirm the target path exists.
- Check broken internal markdown links.
- Check unclosed code fences.
- Check unusually large code fences when readability matters.
- Check table-of-contents link labels against H1 titles.
- Check chapter and appendix backlink consistency.

## Repository-Specific Focus

- For 資料/website/MarkDown, confirm navigation integrity for cover, table-of-contents, chapters, and appendices.
- For review and planning documents, prioritize broken links and malformed markdown over backlink rules.
- For manuscript-related directories, focus on publication-blocking formatting issues first.

## Severity Guide

- Blocking: broken links, missing files, malformed code fences, missing table-of-contents targets
- Important: title mismatch, missing backlink, inconsistent navigation patterns
- Minor: readability issues that do not block review or publication

## Suggested Summary Format

1. Scope audited
2. Overall result
3. Blocking findings
4. Important findings
5. Minor findings
6. Next actions