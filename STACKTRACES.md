# Plan: Implement Stack Traces for Error Handling

## Problem

Feather's error handling is missing automatic stack trace accumulation. When an error propagates up through procedure calls, TCL should build:

- `-errorinfo`: Human-readable stack trace
- `-errorstack`: Machine-readable `{INNER cmd CALL {proc args} ...}` list
- `-errorline`: Line number where error originated

## Architecture Decision

**Store error state as TCL variables in `::tcl::errors` namespace:**

- `::tcl::errors::info` - accumulates errorinfo string
- `::tcl::errors::stack` - accumulates errorstack list
- `::tcl::errors::line` - original error line number
- `::tcl::errors::active` - "1" if error propagation in progress, "0" otherwise

**Benefits:**
- No new Go struct fields
- Uses existing variable infrastructure
- Error state is introspectable from TCL scripts
- State automatically persists across callbacks

---

## Implementation Steps

### Step 1: Create `::tcl::errors` Namespace

**File: `feather.go`** - In `New()`:

```go
func New() *Interp {
    // ... existing init ...

    // Create ::tcl namespace and ::tcl::errors child
    tclNS := &Namespace{
        fullPath: "::tcl",
        parent:   globalNS,
        children: make(map[string]*Namespace),
        vars:     make(map[string]*Obj),
        commands: make(map[string]*Command),
    }
    globalNS.children["tcl"] = tclNS
    interp.namespaces["::tcl"] = tclNS

    errorsNS := &Namespace{
        fullPath: "::tcl::errors",
        parent:   tclNS,
        children: make(map[string]*Namespace),
        vars:     make(map[string]*Obj),
        commands: make(map[string]*Command),
    }
    tclNS.children["errors"] = errorsNS
    interp.namespaces["::tcl::errors"] = errorsNS

    // Initialize error tracking variables
    errorsNS.vars["active"] = interp.String("0")
    errorsNS.vars["info"] = interp.String("")
    errorsNS.vars["stack"] = interp.List()
    errorsNS.vars["line"] = interp.Int(0)

    return interp
}
```

### Step 2: Helper Functions for Error State

**File: `interp_callbacks.go`**

```go
func (i *Interp) errorNS() *Namespace {
    return i.namespaces["::tcl::errors"]
}

func (i *Interp) isErrorActive() bool {
    v := i.errorNS().vars["active"]
    return v != nil && v.String() == "1"
}

func (i *Interp) setErrorActive(active bool) {
    if active {
        i.errorNS().vars["active"] = i.String("1")
    } else {
        i.errorNS().vars["active"] = i.String("0")
    }
}

func (i *Interp) appendErrorInfo(text string) {
    ns := i.errorNS()
    current := ns.vars["info"].String()
    ns.vars["info"] = i.String(current + text)
}

func (i *Interp) appendErrorStack(entries ...*Obj) {
    ns := i.errorNS()
    current, _ := AsList(ns.vars["stack"])
    ns.vars["stack"] = i.List(append(current, entries...)...)
}

func (i *Interp) setErrorLine(line int) {
    i.errorNS().vars["line"] = i.Int(int64(line))
}

func (i *Interp) clearErrorState() {
    ns := i.errorNS()
    ns.vars["active"] = i.String("0")
    ns.vars["info"] = i.String("")
    ns.vars["stack"] = i.List()
    ns.vars["line"] = i.Int(0)
}
```

### Step 3: Detect Error Start in goInterpSetReturnOptions

**File: `interp_callbacks.go`**

```go
//export goInterpSetReturnOptions
func goInterpSetReturnOptions(interp C.FeatherInterp, options C.FeatherObj) C.FeatherResult {
    i := getInterp(interp)
    // ... existing code to store returnOptions ...

    // Detect error start: code == 1 and not already active
    opts := i.objForHandle(FeatherObj(options))
    if code := getCodeFromOptions(opts); code == 1 && !i.isErrorActive() {
        i.initErrorState(opts)
    }

    return C.TCL_OK
}

func (i *Interp) initErrorState(opts *Obj) {
    i.setErrorActive(true)

    // Get initial errorinfo
    if info := ObjDictGet(opts, "-errorinfo"); info != nil {
        i.errorNS().vars["info"] = i.String(info.String())
    } else {
        // Build initial errorinfo from result + current frame
        msg := i.result.String()
        if len(i.frames) > 0 && i.frames[i.active].cmd != nil {
            frame := i.frames[i.active]
            msg += "\n    while executing\n\"" + i.formatCommand(frame.cmd, frame.args) + "\""
        }
        i.errorNS().vars["info"] = i.String(msg)
    }

    // Initialize errorstack with INNER marker
    if len(i.frames) > 0 && i.frames[i.active].cmd != nil {
        frame := i.frames[i.active]
        cmdList := i.buildCallEntry(frame.cmd, frame.args)
        i.errorNS().vars["stack"] = i.List(i.String("INNER"), cmdList)
    } else {
        i.errorNS().vars["stack"] = i.List()
    }

    // Save errorline from current frame
    if len(i.frames) > 0 {
        i.setErrorLine(i.frames[i.active].line)
    }
}
```

### Step 4: Accumulate in goFramePop

**File: `interp_callbacks.go`**

```go
//export goFramePop
func goFramePop(interp C.FeatherInterp) C.FeatherResult {
    i := getInterp(interp)
    // ... existing validation ...

    // If error in progress, append frame to stack trace BEFORE popping
    if i.isErrorActive() {
        frame := i.frames[i.active]
        if frame.cmd != nil {
            // Append to ::tcl::errors::info
            i.appendErrorInfo(fmt.Sprintf("\n    (procedure \"%s\" line %d)",
                frame.cmd.String(), frame.line))
            i.appendErrorInfo(fmt.Sprintf("\n    invoked from within\n\"%s\"",
                i.formatCommand(frame.cmd, frame.args)))

            // Append CALL to ::tcl::errors::stack
            callEntry := i.buildCallEntry(frame.cmd, frame.args)
            i.appendErrorStack(i.String("CALL"), callEntry)
        }
    }

    // ... existing pop logic ...
}
```

### Step 5: Finalize in goInterpGetReturnOptions

**File: `interp_callbacks.go`**

```go
//export goInterpGetReturnOptions
func goInterpGetReturnOptions(interp C.FeatherInterp, code C.FeatherResult) C.FeatherObj {
    i := getInterp(interp)
    // ... existing code ...

    // Finalize error state if error in progress
    if i.isErrorActive() && code == C.TCL_ERROR {
        i.finalizeError()
    }

    return i.handleForObj(i.returnOptions)
}

func (i *Interp) finalizeError() {
    ns := i.errorNS()

    if i.returnOptions == nil {
        i.returnOptions = i.Dict()
    }

    // Copy from ::tcl::errors::* to return options
    ObjDictSet(i.returnOptions, "-errorinfo", ns.vars["info"])
    ObjDictSet(i.returnOptions, "-errorstack", ns.vars["stack"])
    ObjDictSet(i.returnOptions, "-errorline", ns.vars["line"])

    // Set global variables (::errorInfo and ::errorCode)
    i.globalNamespace.vars["errorInfo"] = ns.vars["info"]
    if errorCode := ObjDictGet(i.returnOptions, "-errorcode"); errorCode != nil {
        i.globalNamespace.vars["errorCode"] = errorCode
    } else {
        i.globalNamespace.vars["errorCode"] = i.String("NONE")
    }

    // Clear error state
    i.clearErrorState()
}
```

### Step 6: Helper Functions

**File: `interp_callbacks.go`**

```go
func (i *Interp) formatCommand(cmd, args *Obj) string {
    if args == nil {
        return cmd.String()
    }
    result := cmd.String()
    if list, _ := AsList(args); list != nil {
        for _, arg := range list {
            result += " " + arg.String()
        }
    }
    return result
}

func (i *Interp) buildCallEntry(cmd, args *Obj) *Obj {
    items := []*Obj{cmd}
    if list, _ := AsList(args); list != nil {
        items = append(items, list...)
    }
    return i.List(items...)
}

func getCodeFromOptions(opts *Obj) int {
    if opts == nil {
        return 0
    }
    if codeObj := ObjDictGet(opts, "-code"); codeObj != nil {
        if code, err := AsInt(codeObj); err == nil {
            return int(code)
        }
    }
    return 0
}
```

---

## Files to Modify

| File | Changes |
|------|---------|
| `feather.go` | Create `::tcl::errors` namespace in `New()` |
| `interp_callbacks.go` | Add helpers, modify callbacks |

**No C files need to be modified.**

---

## TCL Introspection

Scripts can now inspect error state:

```tcl
# Check if error propagation is active
set ::tcl::errors::active

# Read accumulated stack trace so far
set ::tcl::errors::info

# Read errorstack list
set ::tcl::errors::stack
```

---

## Expected Output

```tcl
proc foo {} { bar }
proc bar {} { error "oops" }
catch {foo} result opts
puts [dict get $opts -errorinfo]
```

Output:
```
oops
    while executing
"error oops"
    (procedure "bar" line 1)
    invoked from within
"bar"
    (procedure "foo" line 1)
    invoked from within
"foo"
```

---

## Testing

1. Add test case in `testcases/` for stack traces
2. Verify `-errorinfo` format matches TCL 8.5+
3. Verify `-errorstack` contains CALL/INNER entries
4. Verify `-errorline` is populated
5. Verify `::errorInfo` and `::errorCode` globals are set
6. Verify `::tcl::errors::*` variables are accessible
7. Run `mise test` to ensure no regressions
