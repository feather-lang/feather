# Feature Queue

This document defines the ordered list of features to implement, their dependencies, and acceptance criteria.

## Feature Dependency Graph

```
                                    ┌─────────┐
                                    │  lexer  │
                                    └────┬────┘
                                         │
                                    ┌────▼────┐
                                    │ parser  │
                                    └────┬────┘
                                         │
                    ┌────────────────────┼────────────────────┐
                    │                    │                    │
               ┌────▼────┐         ┌─────▼─────┐        ┌─────▼─────┐
               │  subst  │         │ variables │        │  commands │
               └────┬────┘         └─────┬─────┘        └─────┬─────┘
                    │                    │                    │
                    └────────────────────┼────────────────────┘
                                         │
                                    ┌────▼────┐
                                    │  eval   │
                                    └────┬────┘
                                         │
              ┌──────────────────────────┼──────────────────────────┐
              │                          │                          │
         ┌────▼────┐               ┌─────▼─────┐              ┌─────▼─────┐
         │  expr   │               │  control  │              │   proc    │
         └────┬────┘               └─────┬─────┘              └─────┬─────┘
              │                          │                          │
              │                          │                    ┌─────▼─────┐
              │                          │                    │   scope   │
              │                          │                    │(upvar/up- │
              │                          │                    │  level)   │
              │                          │                    └─────┬─────┘
              │                          │                          │
              └──────────────────────────┼──────────────────────────┘
                                         │
                              ┌──────────┼──────────┐
                              │          │          │
                        ┌─────▼───┐ ┌────▼────┐ ┌───▼────┐
                        │  list   │ │ string  │ │  dict  │
                        └─────────┘ └─────────┘ └────────┘
                                         │
                              ┌──────────┼──────────┐
                              │          │          │
                        ┌─────▼───┐ ┌────▼────┐ ┌───▼────┐
                        │  array  │ │  error  │ │   io   │
                        └─────────┘ └─────────┘ └────────┘
                                         │
                              ┌──────────┼──────────┐
                              │          │          │
                        ┌─────▼───┐ ┌────▼────┐ ┌───▼─────┐
                        │  event  │ │  interp │ │coroutine│
                        └─────────┘ └─────────┘ └─────────┘
```

## Phase 1: Core Language

### 1.1 lexer

**Description:** Tokenize TCL source into words, respecting quoting rules.

**Dependencies:** None

**Key behaviors:**
- Word boundaries (whitespace, semicolon, newline)
- Brace quoting `{...}` - no substitution, nesting
- Double quote quoting `"..."` - substitution occurs
- Backslash-newline continuation
- Comments (`#` at command start)

**Tests:**
- `lexer-1.*` - Basic word splitting
- `lexer-2.*` - Brace quoting
- `lexer-3.*` - Double quote quoting
- `lexer-4.*` - Backslash sequences
- `lexer-5.*` - Comments
- `lexer-6.*` - Edge cases

**Source files:**
- `core/lexer.c`

---

### 1.2 parser

**Description:** Parse token stream into command structure.

**Dependencies:** `lexer`

**Key behaviors:**
- Command = list of words
- Script = list of commands separated by newline/semicolon
- Track source locations for error messages

**Tests:**
- `parser-1.*` - Single commands
- `parser-2.*` - Multi-command scripts
- `parser-3.*` - Nested structures

**Source files:**
- `core/parser.c`

---

### 1.3 subst

**Description:** Variable, command, and backslash substitution.

**Dependencies:** `lexer`, `parser`

**Key behaviors:**
- `$name` and `${name}` variable substitution
- `$arr(key)` array element substitution
- `[cmd args]` command substitution (recursive eval)
- Backslash escapes: `\n`, `\t`, `\xHH`, `\uHHHH`, `\\`, `\$`, `\[`
- Substitution in double quotes, not in braces

**Tests:**
- `subst-1.*` - Variable substitution
- `subst-2.*` - Command substitution
- `subst-3.*` - Backslash substitution
- `subst-4.*` - Mixed substitution
- `subst-5.*` - Edge cases

**Source files:**
- `core/subst.c`

---

### 1.4 variables

**Description:** Variable storage and retrieval.

**Dependencies:** `lexer`, `parser`

**Key behaviors:**
- `set` command (read and write)
- `unset` command
- Frame-local variables
- Global frame access

**Tests:**
- `var-1.*` - Basic set/get
- `var-2.*` - Unset
- `var-3.*` - Non-existent variables

**Source files:**
- `core/builtins.c` (set, unset commands)

---

### 1.5 commands

**Description:** Command dispatch infrastructure.

**Dependencies:** `lexer`, `parser`

**Key behaviors:**
- Builtin lookup (C table)
- Proc lookup (host table)
- Extension lookup (host callback)
- Unknown command handler

**Tests:**
- `cmd-1.*` - Builtin dispatch
- `cmd-2.*` - Unknown command error
- `cmd-3.*` - Rename command

**Source files:**
- `core/eval.c`

---

### 1.6 eval

**Description:** Command evaluation trampoline.

**Dependencies:** `subst`, `variables`, `commands`

**Key behaviors:**
- Non-recursive evaluation (for coroutine support)
- Proper result propagation
- Error handling

**Tests:**
- `eval-1.*` - Basic evaluation
- `eval-2.*` - Nested evaluation
- `eval-3.*` - eval command

**Source files:**
- `core/eval.c`

---

## Phase 2: Control Flow and Procedures

### 2.1 expr

**Description:** Expression parser and evaluator.

**Dependencies:** `eval`

**Key behaviors:**
- Arithmetic: `+ - * / %`
- Comparison: `== != < > <= >=`
- Logical: `&& || !`
- Bitwise: `& | ^ ~ << >>`
- Math functions: `sin cos sqrt pow abs int double round`
- String comparison: `eq ne`
- Ternary: `? :`
- Parentheses for grouping

**Tests:**
- `expr-1.*` - Arithmetic
- `expr-2.*` - Comparison
- `expr-3.*` - Logical
- `expr-4.*` - Functions
- `expr-5.*` - Precedence
- `expr-6.*` - Edge cases

**Source files:**
- `core/expr.c`

---

### 2.2 control

**Description:** Control flow commands.

**Dependencies:** `eval`, `expr`

**Key behaviors:**
- `if`/`elseif`/`else`
- `while`
- `for`
- `foreach`
- `switch`
- `break`, `continue`

**Tests:**
- `if-*` - Conditional execution
- `while-*` - While loops
- `for-*` - For loops
- `foreach-*` - List iteration
- `switch-*` - Switch statement
- `break-*` - Break from loops
- `continue-*` - Continue in loops

**Source files:**
- `core/builtins.c`

---

### 2.3 proc

**Description:** Procedure definition and invocation.

**Dependencies:** `eval`

**Key behaviors:**
- `proc name args body`
- Argument binding
- Default argument values
- `args` for varargs
- `return` command
- Proper frame creation

**Tests:**
- `proc-1.*` - Basic procedures
- `proc-2.*` - Arguments
- `proc-3.*` - Default values
- `proc-4.*` - Varargs
- `proc-5.*` - Return values

**Source files:**
- `core/builtins.c`
- `core/eval.c`

---

### 2.4 scope

**Description:** Scope manipulation commands.

**Dependencies:** `proc`

**Key behaviors:**
- `upvar` - Link to variable in calling frame
- `uplevel` - Execute in calling frame
- `global` - Link to global variable
- `variable` - Namespace variable (simplified)

**Tests:**
- `upvar-*` - Upvar linking
- `uplevel-*` - Uplevel execution
- `global-*` - Global access

**Source files:**
- `core/builtins.c`

---

## Phase 3: Data Structures

### 3.1 list

**Description:** List manipulation commands.

**Dependencies:** `eval`

**Key behaviors:**
- `list` - Create list
- `lindex` - Get element
- `llength` - Get length
- `lrange` - Get range
- `lappend` - Append to list variable
- `linsert` - Insert elements
- `lreplace` - Replace elements
- `lset` - Set element
- `lsort` - Sort list
- `lsearch` - Search list
- `lmap` - Map over list
- `concat` - Concatenate lists
- `join` - Join with separator
- `split` - Split string into list

**Tests:**
- `list-*` - All list operations

**Source files:**
- `core/builtins.c`

---

### 3.2 string

**Description:** String manipulation commands.

**Dependencies:** `eval`

**Key behaviors:**
- `string length`
- `string index`
- `string range`
- `string compare`
- `string equal`
- `string match` (glob)
- `string first`, `string last`
- `string map`
- `string tolower`, `string toupper`
- `string trim`, `string trimleft`, `string trimright`
- `string replace`
- `string repeat`
- `string reverse`
- `string is` (type checking)
- `format` (sprintf-style)
- `scan` (sscanf-style)
- `append`

**Tests:**
- `string-*` - All string operations
- `format-*` - Format command
- `scan-*` - Scan command

**Source files:**
- `core/builtins.c`
- `core/format.c`

---

### 3.3 dict

**Description:** Dictionary manipulation commands.

**Dependencies:** `eval`

**Key behaviors:**
- `dict create`
- `dict get`, `dict set`
- `dict exists`
- `dict keys`, `dict values`
- `dict remove`
- `dict size`
- `dict for`
- `dict map`
- `dict filter`
- `dict merge`

**Tests:**
- `dict-*` - All dict operations

**Source files:**
- `core/builtins.c`

---

### 3.4 array

**Description:** Array manipulation commands.

**Dependencies:** `variables`

**Key behaviors:**
- `array set`
- `array get`
- `array names`
- `array size`
- `array exists`
- `array unset`
- `parray` (for debugging)

**Tests:**
- `array-*` - All array operations

**Source files:**
- `core/builtins.c`

---

## Phase 4: Error Handling and I/O

### 4.1 error

**Description:** Error handling commands.

**Dependencies:** `eval`

**Key behaviors:**
- `catch` - Catch errors
- `try`/`on`/`trap`/`finally` - Structured error handling
- `throw` - Throw error with code
- `error` - Generate error
- `return -code error` - Return as error
- Error info and error code propagation

**Tests:**
- `catch-*` - Catch errors
- `try-*` - Try/finally
- `error-*` - Error generation
- `throw-*` - Throw errors

**Source files:**
- `core/builtins.c`
- `core/eval.c`

---

### 4.2 io

**Description:** I/O commands.

**Dependencies:** `eval`

**Key behaviors:**
- `puts`, `gets`
- `open`, `close`
- `read`
- `seek`, `tell`, `eof`
- `flush`
- `fconfigure`
- `chan` ensemble

**Tests:**
- `io-*` - All I/O operations
- `chan-*` - Channel operations

**Source files:**
- `core/builtins.c`

---

## Phase 5: Advanced Features

### 5.1 event

**Description:** Event loop commands.

**Dependencies:** `io`

**Key behaviors:**
- `after` - Timer events
- `vwait` - Wait for variable
- `fileevent` - Channel events
- `update` - Process pending events

**Tests:**
- `after-*` - Timer events
- `vwait-*` - Variable wait
- `fileevent-*` - Channel events

**Source files:**
- `core/builtins.c`

---

### 5.2 interp

**Description:** Multiple interpreter support.

**Dependencies:** `eval`

**Key behaviors:**
- `interp create`
- `interp eval`
- `interp alias`
- `interp delete`
- `interp share`, `interp transfer`
- Safe interpreters

**Tests:**
- `interp-*` - All interpreter operations

**Source files:**
- `core/interp.c`
- `core/builtins.c`

---

### 5.3 coroutine

**Description:** Coroutine support.

**Dependencies:** `eval`, `proc`

**Key behaviors:**
- `coroutine` - Create coroutine
- `yield` - Yield value
- `yieldto` - Yield to specific command
- Coroutine resume on command invocation
- Proper stack save/restore

**Tests:**
- `coro-*` - All coroutine operations

**Source files:**
- `core/coro.c`
- `core/eval.c`

---

## Phase 6: System Integration

### 6.1 exec

**Description:** Subprocess execution.

**Dependencies:** `io`

**Key behaviors:**
- `exec` command
- Pipeline support (`|`)
- Redirection (`<`, `>`, `2>`, `>&`)
- Background execution (`&`)
- `pid` command

**Tests:**
- `exec-*` - Process execution

**Source files:**
- `core/builtins.c`

---

### 6.2 file

**Description:** File system commands.

**Dependencies:** `io`

**Key behaviors:**
- `file exists`, `file isfile`, `file isdirectory`
- `file size`, `file mtime`
- `file delete`, `file rename`, `file copy`, `file mkdir`
- `file dirname`, `file tail`, `file extension`, `file rootname`
- `file join`, `file split`, `file normalize`
- `glob`
- `cd`, `pwd`

**Tests:**
- `file-*` - All file operations

**Source files:**
- `core/builtins.c`

---

### 6.3 socket

**Description:** Network socket support.

**Dependencies:** `io`, `event`

**Key behaviors:**
- `socket` client connection
- `socket -server` for server sockets
- Async connection support
- Integration with event loop

**Tests:**
- `socket-*` - Socket operations

**Source files:**
- `core/builtins.c`

---

### 6.4 regex

**Description:** Regular expression support.

**Dependencies:** `eval`

**Key behaviors:**
- `regexp` - Match patterns
- `regsub` - Substitute matches
- Capture groups
- Various flags (`-nocase`, `-all`, `-inline`, etc.)

**Tests:**
- `regexp-*` - Pattern matching
- `regsub-*` - Substitution

**Source files:**
- `core/builtins.c`

---

### 6.5 info

**Description:** Introspection commands.

**Dependencies:** All previous

**Key behaviors:**
- `info commands`, `info procs`
- `info vars`, `info locals`, `info globals`
- `info exists`
- `info body`, `info args`, `info default`
- `info level`, `info frame`
- `info script`
- `info patchlevel`, `info tclversion`
- `info coroutine`

**Tests:**
- `info-*` - All introspection

**Source files:**
- `core/builtins.c`

---

### 6.6 namespace

**Description:** Namespace support.

**Dependencies:** `eval`, `proc`, `variables`

**Key behaviors:**
- `namespace eval`
- `namespace current`
- `namespace export`, `namespace import`
- `namespace exists`
- `namespace children`, `namespace parent`
- Qualified names (`::foo::bar`)
- Variable resolution order

**Tests:**
- `namespace-*` - All namespace operations

**Source files:**
- `core/namespace.c`
- `core/eval.c`

---

## Feature YAML Schema

```yaml
features:
  - id: string           # Unique identifier
    description: string  # Human-readable description
    depends: [string]    # List of dependency feature IDs
    phase: int           # Implementation phase (1-6)
    status: string       # pending | in_progress | complete
    tests: [string]      # Test file patterns
    source_files:        # Files to modify
      - path: string
        description: string
```
