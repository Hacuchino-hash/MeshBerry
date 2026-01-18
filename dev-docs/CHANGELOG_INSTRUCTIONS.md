# MeshBerry Development Changelog System

## MANDATORY DOCUMENTATION REQUIREMENT

**EVERY change to this codebase MUST be documented.** This applies to:

- Human developers
- AI assistants (Claude, Copilot, Cursor, ChatGPT, etc.)
- Automated tools and scripts

**NO EXCEPTIONS.** Undocumented changes will be rejected.

---

## Directory Structure

```
dev-docs/
├── REQUIREMENTS.md              # Project requirements specification
├── CHANGELOG_INSTRUCTIONS.md    # This file - how to document changes
├── CONTRIBUTING.md              # Contribution guidelines
├── changes/                     # Per-feature/fix changelog entries
│   ├── YYYYMMDD-short-name.md   # Individual change files
│   └── ...
└── templates/
    └── CHANGE_TEMPLATE.md       # Template for new changelog entries
```

---

## How to Document Changes

### Step 1: Create a New Change File

For **every** feature, fix, refactor, or modification:

1. Navigate to `dev-docs/changes/`
2. Create a new file named: `YYYYMMDD-short-descriptive-name.md`
   - Example: `20260117-add-keyboard-driver.md`
   - Example: `20260118-fix-lora-timeout.md`

### Step 2: Use the Template

Copy the template from `dev-docs/templates/CHANGE_TEMPLATE.md` and fill in ALL fields.

### Step 3: Be Thorough

Document EVERYTHING:
- What was changed
- Why it was changed
- How it was implemented
- What files were modified
- What was tested
- What results were observed
- Any known issues or follow-ups

---

## Required Fields in Change Documentation

| Field | Required | Description |
|-------|----------|-------------|
| Title | YES | Brief description of the change |
| Date | YES | YYYY-MM-DD format |
| Author | YES | Name or identifier (human or AI) |
| Type | YES | feature / fix / refactor / docs / test / config |
| Status | YES | completed / in-progress / reverted |
| Related Issues | NO | GitHub issue numbers if applicable |
| Files Modified | YES | Complete list of changed files |
| Summary | YES | What was done and why |
| Technical Details | YES | How it was implemented |
| Testing | YES | What was tested and results |
| Breaking Changes | YES | Any breaking changes (or "None") |
| Known Issues | NO | Any remaining issues |
| Follow-up Tasks | NO | Future work needed |

---

## AI-Specific Instructions

### For Claude, Copilot, Cursor, ChatGPT, and Other AI Assistants

When making ANY changes to this codebase, you MUST:

1. **BEFORE making changes:**
   - Read this file (`dev-docs/CHANGELOG_INSTRUCTIONS.md`)
   - Read `dev-docs/REQUIREMENTS.md` to understand project context
   - Review recent changes in `dev-docs/changes/` to understand current state

2. **AFTER making changes:**
   - Create a changelog entry in `dev-docs/changes/`
   - Use the template from `dev-docs/templates/CHANGE_TEMPLATE.md`
   - Document ALL files you modified
   - Include your AI model identifier as the author
   - Be specific about what you changed and why

3. **Documentation Standards:**
   - Use clear, technical language
   - Include code snippets where helpful
   - Reference line numbers when discussing specific changes
   - Explain your reasoning for implementation choices
   - Note any assumptions you made

4. **Example Author Identifiers:**
   - `Claude (claude-3-opus)`
   - `GitHub Copilot`
   - `Cursor AI`
   - `ChatGPT-4`
   - `Human: [Name]`

### AI Handoff Protocol

When an AI session ends or a new AI takes over:

1. The outgoing AI must document:
   - Current state of work
   - What was completed
   - What remains to be done
   - Any blockers or issues encountered

2. The incoming AI must:
   - Read all recent changelog entries
   - Understand the current state
   - Continue from where the previous AI left off
   - Create new changelog entries for their work

---

## File Naming Convention

Format: `YYYYMMDD-short-name.md`

### Examples:

| Filename | Description |
|----------|-------------|
| `20260117-initial-project-setup.md` | Initial project structure |
| `20260117-add-keyboard-i2c-driver.md` | Keyboard driver implementation |
| `20260118-fix-display-init-sequence.md` | Display initialization fix |
| `20260118-refactor-mesh-packet-handler.md` | Refactoring mesh code |
| `20260119-add-spanish-localization.md` | Spanish language support |

### Multiple Changes Same Day:

If multiple unrelated changes occur on the same day, create separate files:
- `20260117-add-keyboard-driver.md`
- `20260117-add-trackball-input.md`
- `20260117-fix-spi-conflict.md`

---

## What to Document

### ALWAYS Document:

- New features added
- Bug fixes
- Refactoring
- Configuration changes
- Dependency additions/updates
- Build system changes
- Test additions/modifications
- Documentation updates
- Performance optimizations
- Security fixes

### Include These Details:

- Exact file paths modified
- Functions/classes added or changed
- Configuration values changed
- Dependencies added (with versions)
- Test commands run
- Test results (pass/fail, output)
- Error messages encountered
- Solutions applied

---

## Verification Checklist

Before committing, verify your changelog entry includes:

- [ ] Correct date in filename and content
- [ ] Your author identifier
- [ ] Change type (feature/fix/refactor/etc.)
- [ ] Complete list of modified files
- [ ] Clear summary of what was done
- [ ] Technical implementation details
- [ ] Testing performed and results
- [ ] Breaking changes noted (or "None")
- [ ] Follow-up tasks if any

---

## Reading Change History

To understand the project's evolution:

1. List all changes chronologically:
   ```bash
   ls -la dev-docs/changes/
   ```

2. Read changes for a specific date:
   ```bash
   cat dev-docs/changes/20260117-*.md
   ```

3. Search for changes to a specific file:
   ```bash
   grep -l "src/drivers/keyboard.cpp" dev-docs/changes/*.md
   ```

4. Find all changes by a specific author:
   ```bash
   grep -l "Author: Claude" dev-docs/changes/*.md
   ```

---

## Integration with Git

This documentation is **in addition to** git commits, not a replacement.

- Git commits: Brief, one-line summaries
- Changelog entries: Detailed documentation with context, reasoning, and results

Both are required for every change.

---

## Enforcement

Pull requests and merges will be rejected if:

- No corresponding changelog entry exists in `dev-docs/changes/`
- Changelog entry is incomplete
- Files modified don't match changelog documentation

---

## Questions?

If unclear about documentation requirements:
1. Read this file again
2. Look at existing entries in `dev-docs/changes/` for examples
3. When in doubt, document MORE rather than less
