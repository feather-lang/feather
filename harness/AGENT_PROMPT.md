# Agent Prompt Template

This document defines the prompt structure for invoking a coding agent to implement or fix TCL features.

## Prompt Template

```markdown
# TCL Core Implementation Task

## Feature: {{feature_id}}

**Description:** {{description}}

**Dependencies:** {{dependencies}}

**Status:** {{pass_count}}/{{total_count}} tests passing

---

## Current Test Results

{{#if all_passing}}
All tests passing. Feature complete.
{{else}}

### Failing Tests

{{#each failing_tests}}
#### Test: {{name}}

**Script:**
```tcl
{{script}}
```

**Expected output:**
```
{{expected_stdout}}
```
{{#if expected_stderr}}
**Expected stderr:**
```
{{expected_stderr}}
```
{{/if}}
**Expected return code:** {{expected_code}}

**Actual output:**
```
{{actual_stdout}}
```
{{#if actual_stderr}}
**Actual stderr:**
```
{{actual_stderr}}
```
{{/if}}
**Actual return code:** {{actual_code}}

**Diff:**
```diff
{{diff}}
```

---
{{/each}}
{{/if}}

## TCL Documentation Excerpt

{{man_page_excerpt}}

## Implementation Guidelines

1. **Match TCL 9 semantics exactly** - The oracle (tclsh9) is the source of truth
2. **No memory allocation in C** - Use host callbacks for all dynamic memory
3. **Error messages must match** - TCL has specific error message formats
4. **Handle edge cases** - Empty strings, negative indices, Unicode, etc.

## Files You May Modify

{{#each source_files}}
- `{{path}}` - {{description}}
{{/each}}

## Files for Reference (Read-Only)

- `core/tclc.h` - Host interface definition
- `spec/features/{{feature_id}}.md` - Feature specification
- `harness/oracle/{{feature_id}}.json` - Expected test outputs

## Your Task

1. Analyze the failing tests and identify the root cause
2. Implement or fix the relevant code in the C core
3. Ensure your changes don't break other tests
4. Run `make test-c` for fast feedback on C changes
5. Run `make diff FEATURE={{feature_id}}` to check against oracle

Focus on making tests pass one at a time. Commit working increments.
```

## Variable Definitions

| Variable | Description |
|----------|-------------|
| `feature_id` | Feature identifier (e.g., `subst`, `expr`) |
| `description` | Human-readable feature description |
| `dependencies` | List of features this depends on |
| `pass_count` | Number of passing tests |
| `total_count` | Total number of tests |
| `failing_tests` | Array of failing test objects |
| `name` | Test name/identifier |
| `script` | TCL script being tested |
| `expected_stdout` | Expected standard output |
| `expected_stderr` | Expected standard error (may be empty) |
| `expected_code` | Expected return code (0-4) |
| `actual_stdout` | Actual standard output |
| `actual_stderr` | Actual standard error |
| `actual_code` | Actual return code |
| `diff` | Unified diff between expected and actual |
| `man_page_excerpt` | Relevant TCL documentation |
| `source_files` | List of files the agent may modify |

## Example Instantiated Prompt

```markdown
# TCL Core Implementation Task

## Feature: subst

**Description:** Variable, command, and backslash substitution

**Dependencies:** lexer, parser

**Status:** 12/15 tests passing

---

## Current Test Results

### Failing Tests

#### Test: subst-1.7

**Script:**
```tcl
set x "world"
subst {Hello $x}
```

**Expected output:**
```
Hello world
```

**Expected return code:** 0

**Actual output:**
```
Hello $x
```

**Actual return code:** 0

**Diff:**
```diff
-Hello world
+Hello $x
```

---

#### Test: subst-2.3

**Script:**
```tcl
subst {[expr {1 + 2}]}
```

**Expected output:**
```
3
```

**Expected return code:** 0

**Actual output:**
```
[expr {1 + 2}]
```

**Actual return code:** 0

**Diff:**
```diff
-3
+[expr {1 + 2}]
```

---

## TCL Documentation Excerpt

**subst** ?**-nobackslashes**? ?**-nocommands**? ?**-novariables**? *string*

This command performs variable substitutions, command substitutions, and
backslash substitutions on *string*, returning the result.

Variable substitution: `$name` or `${name}` is replaced with the value
of variable *name*.

Command substitution: `[cmd args]` is replaced with the result of
evaluating *cmd args*.

Backslash substitution: `\n`, `\t`, etc. are replaced with their
corresponding characters.

## Implementation Guidelines

1. **Match TCL 9 semantics exactly** - The oracle (tclsh9) is the source of truth
2. **No memory allocation in C** - Use host callbacks for all dynamic memory
3. **Error messages must match** - TCL has specific error message formats
4. **Handle edge cases** - Empty strings, negative indices, Unicode, etc.

## Files You May Modify

- `core/subst.c` - Substitution implementation
- `core/eval.c` - May need changes for command subst

## Your Task

1. Analyze the failing tests and identify the root cause
2. Implement or fix the relevant code in the C core
3. Ensure your changes don't break other tests
4. Run `make test-c` for fast feedback on C changes
5. Run `make diff FEATURE=subst` to check against oracle

Focus on making tests pass one at a time. Commit working increments.
```

## Prompt Generation Script

```python
#!/usr/bin/env python3
# harness/gen_prompt.py

import json
import yaml
from pathlib import Path
from jinja2 import Template

def load_feature(feature_id: str) -> dict:
    with open(f'spec/features.yaml') as f:
        features = yaml.safe_load(f)['features']
    return next(f for f in features if f['id'] == feature_id)

def load_test_results(feature_id: str) -> dict:
    with open(f'harness/results/{feature_id}.json') as f:
        return json.load(f)

def load_man_page(feature_id: str) -> str:
    path = Path(f'spec/features/{feature_id}.md')
    if path.exists():
        return path.read_text()
    return "(No documentation available)"

def generate_prompt(feature_id: str) -> str:
    template = Template(PROMPT_TEMPLATE)
    
    feature = load_feature(feature_id)
    results = load_test_results(feature_id)
    man_page = load_man_page(feature_id)
    
    failing = [t for t in results['tests'] if not t['passed']]
    passing = [t for t in results['tests'] if t['passed']]
    
    return template.render(
        feature_id=feature_id,
        description=feature['description'],
        dependencies=', '.join(feature.get('depends', [])),
        pass_count=len(passing),
        total_count=len(results['tests']),
        all_passing=len(failing) == 0,
        failing_tests=failing,
        man_page_excerpt=man_page,
        source_files=feature.get('source_files', [])
    )

if __name__ == '__main__':
    import sys
    feature_id = sys.argv[1]
    print(generate_prompt(feature_id))
```

## Usage

```bash
# Generate prompt for a feature
python3 harness/gen_prompt.py subst > prompt.md

# Invoke agent with prompt
claude --prompt prompt.md

# Or with other agents:
aider --message-file prompt.md
```
