---
name: commit
description: Create a git commit at the end of a work increment. Use when you have completed a meaningful unit of work and need to hand off to a colleague.
---

# Commit Skill

This skill guides you through creating commits that serve as handoff documents for your colleagues.

## When to Commit

Commit at the end of every meaningful increment of work. A commit marks the boundary between your work session and your colleague's.

**Your colleague will only have access to the commit message** - they cannot ask you questions, so the message must be self-contained.

## Commit Message Structure

Use this structure:

```
<summary line: what was done>

<context and rationale>

Architecture decisions:
- Key design choices made
- Trade-offs considered

What was implemented:
- List of concrete changes
- New files, modified behavior

Current state:
<describe what works, what doesn't, any known issues>

Next steps for colleague:
1. Immediate next task
2. Follow-up work
3. Things to watch out for
```

## Commit Process

1. **Stage changes intentionally**
   - Review what you're committing with `git status` and `git diff`
   - Never commit secrets, credentials, or `.env` files

2. **Write a comprehensive message**
   - First line: imperative mood, ~50 chars ("Implement parser", not "Implemented parser")
   - Include your reasoning, not just what changed
   - Document dead ends and failed approaches if relevant
   - State assumptions that might need revisiting

## Example Commit

```
Implement test harness for tclc

The harness is a data-driven test runner that executes TCL scripts
against host implementations and validates the output.

Architecture decisions:
- Thin CLI entrypoint using spf13/cobra for argument parsing
- All logic lives in the harness package, not main.go
- Tests are defined as XML/HTML files with <test-suite>/<test-case> structure

Package structure:
- test_model.go: TestCase and TestSuite data types
- parser.go: XML parsing (ParseFile, Parse functions)
- collector.go: Recursive test file discovery (CollectTestFiles)
- runner.go: Test execution against host executable

Current state: Core infrastructure complete. Ready for integration
with an actual tclc host implementation.

Next steps for colleague:
1. Implement a minimal host that can run the simple-command-invocation test
2. Update mise.toml to wire up the test task properly
3. Consider adding support for the <result> field (TCL_OK/TCL_ERROR)
```

## Important Reminders

- **Never amend commits that have been pushed** unless explicitly asked
- **Never force push to main/master**
- **Never skip hooks** (--no-verify) unless explicitly requested
- Run `mise build` and `mise test` before committing when you've changed code
- If tests fail, fix them before committing or document why they're failing

## Before Starting Work

As per the project's working process, always run `git show HEAD` before starting any task to understand the current state and your colleague's handoff notes.
