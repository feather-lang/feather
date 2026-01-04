# Feather Interpreter State Persistence - Feasibility Report

## Executive Summary

Implementing interpreter state persistence is **feasible** for both Go and JavaScript hosts, with well-defined challenges around foreign types and commands. The existing architecture already supports clean state boundaries through:

1. **Dual-arena object model** - separating temporary (scratch) from permanent storage
2. **Unified namespace storage** - persistent variables already materialized
3. **Procedure definitions** - stored as immutable objects

The primary challenge is **foreign command/type resurrection** - handling cases where custom Go/JS types may not be registered when the state is restored.

## Current State Architecture

### What Constitutes Interpreter State?

#### Persistent State (Survives `eval` boundaries)

| Component | Storage | Go Location | JS Location |
|-----------|---------|-------------|-------------|
| **Namespace hierarchy** | Tree of namespaces with vars | `Interp.namespaces` | `FeatherInterp.namespaces` |
| **Global variables** | In global namespace | `globalNamespace.vars` | `frames[0].vars` |
| **Procedures** | Name, params, body | `Namespace.commands` | Stored in namespace vars |
| **Foreign type definitions** | Constructor, methods, destructor | `ForeignRegistry.types` | `foreignTypes` Map |
| **Foreign instances** | Handle → value mapping | `ForeignRegistry.instances` | `foreignInstances` Map |
| **Registered commands** | Go/JS functions | `Interp.Commands` | `hostCommands` Map |
| **Variable links** | Upvar/variable connections | `CallFrame.links` | Frame-local (not persistent) |
| **Namespace exports** | Export patterns | `Namespace.exportPatterns` | Not yet implemented |
| **Configuration** | Recursion limit, script path | `Interp.recursionLimit` | `recursionLimit` |

#### Temporary State (Reset after top-level eval)

| Component | Lifetime | Go Location | JS Location |
|-----------|----------|-------------|-------------|
| **Call frames** | During execution | `frames` slice, `active` index | `frames` array |
| **Scratch objects** | Single eval | `scratch` map | `scratch.objects` |
| **Current result** | Single eval | `result` | `result` handle |
| **Return options** | Single return | `returnOptions` | `returnOptions` map |
| **Error state** | Single eval | Stored in result | Stored in result |

### Object Handle Model

Both hosts use a **two-arena approach**:

#### Go Host (feather.go:20-26, interp_core.go:134-149)

```go
type Interp struct {
    objects       map[FeatherObj]*Obj  // Permanent (no high bit)
    scratch       map[FeatherObj]*Obj  // Temporary (high bit set)
    scratchNextID FeatherObj           // 1<<63 | counter
    nextID        FeatherObj           // counter (no high bit)
}

const scratchHandleBit = 1 << 63
```

- **Permanent handles**: Start at 1, no high bit (0x0000000000000001, 0x0000000000000002, ...)
- **Scratch handles**: High bit set (0x8000000000000001, 0x8000000000000002, ...)
- **Reset**: `resetScratch()` clears scratch map after each top-level eval

#### JavaScript Host (feather.js:30-77)

```javascript
class FeatherInterp {
    scratch = { objects: new Map(), nextHandle: 1 };
    evalDepth = 0;

    resetScratch() {
        this.scratch = { objects: new Map(), nextHandle: 1 };
    }

    materialize(handle) { /* Deep copy to persistent value */ }
    wrap(value) { /* Wrap persistent value in fresh handle */ }
}
```

- **Handles**: Temporary integers, only valid during single eval
- **Materialization**: Deep-copy objects from handles → persistent JS values
- **Wrapping**: Convert persistent values → fresh handles for next eval

### Foreign Type System

#### Go Host (interp_foreign.go:10-45)

```go
type foreignTypeInfo struct {
    name         string
    newFunc      reflect.Value        // Constructor
    methods      map[string]reflect.Value
    stringRep    reflect.Value        // String representation
    destroy      reflect.Value        // Destructor
    receiverType reflect.Type
}

type foreignInstance struct {
    typeName   string
    handleName string      // "mux1", "connection2", etc.
    objHandle  FeatherObj  // Handle in permanent storage
    value      any         // Actual Go value (*http.ServeMux, *sql.DB, etc.)
}
```

- **Registration**: `interp.RegisterType[T]("TypeName", ...)` stores reflection info
- **Instances**: Created via `TypeName new`, stored with unique handle names
- **Method dispatch**: `$mux1 listen 8080` → lookup instance → call via reflection
- **Lifecycle**: Explicit `$mux1 destroy` removes from registry

#### JavaScript Host (feather.js:1697-1730)

```javascript
foreignTypes = new Map();  // typeName → { new, methods, ... }
foreignInstances = new Map();  // handleName → { typeName, value, objHandle }

registerType(interpId, typeName, typeDef) {
    this.foreignTypes.set(typeName, typeDef);
}

createForeign(interpId, typeName, value, handleName) {
    const objHandle = this.store({ type: 'foreign', typeName, value, stringRep: handleName });
    this.foreignInstances.set(handleName, { typeName, value, objHandle, handleName });
    // Register as command for method dispatch
    this.hostCommands.set(handleName, (args) => { /* method dispatcher */ });
}
```

- **Registration**: `feather.registerType(interpId, 'TypeName', { new, methods })`
- **Instances**: Stored with actual JS values, registered as commands
- **Method dispatch**: Command lookup → method function call with JS value

## Serialization Strategy

### State Snapshot Format

Propose a **JSON-based format** for portability between sessions:

```json
{
  "version": "1.0",
  "featherVersion": "0.1.0",
  "timestamp": "2026-01-04T12:34:56Z",
  "resumeTarget": "/path/to/script.tcl",

  "namespaces": {
    "::": {
      "children": ["foo", "bar"],
      "vars": {
        "myVar": { "type": "string", "value": "hello" },
        "count": { "type": "int", "value": 42 }
      },
      "exports": []
    },
    "::foo": {
      "children": [],
      "vars": {
        "x": { "type": "list", "items": [{"type": "int", "value": 1}] }
      },
      "exports": ["get*"]
    }
  },

  "procedures": {
    "myProc": {
      "namespace": "::",
      "params": ["x", "y"],
      "body": "expr {$x + $y}"
    }
  },

  "foreignTypes": [
    {
      "typeName": "Mux",
      "instances": [
        {
          "handleName": "mux1",
          "serializedState": null,
          "placeholder": true
        }
      ]
    }
  ],

  "config": {
    "recursionLimit": 200
  }
}
```

### Serializable Object Types

| Type | Serialization | Example |
|------|---------------|---------|
| **String** | Direct JSON string | `{"type": "string", "value": "hello"}` |
| **Int** | JSON number | `{"type": "int", "value": 42}` |
| **Double** | JSON number | `{"type": "double", "value": 3.14}` |
| **List** | Recursive array | `{"type": "list", "items": [...]}` |
| **Dict** | Key-value pairs | `{"type": "dict", "entries": [[k, v], ...]}` |
| **Foreign** | **SPECIAL** - see below | `{"type": "foreign", "typeName": "Mux", "handle": "mux1"}` |

### The Foreign Object Challenge

**Problem**: Foreign objects hold arbitrary Go/JS values that cannot be generically serialized:

```go
// Go example
type ServeMux struct { /* contains function pointers, sync.Mutex, etc. */ }
mux := http.NewServeMux()
// How do we serialize mux?
```

```javascript
// JS example
const server = http.createServer();
// How do we serialize server (has callbacks, network state)?
```

**Key insight**: We cannot serialize the **value**, but we can serialize the **identity** and **type**.

## Proposed Solutions for Foreign Objects

### Option 1: Placeholder with Manual Reconstruction (RECOMMENDED)

**Approach**: Serialize foreign object metadata, require host to re-register types on load.

#### Save Phase

1. For each foreign instance, save:
   - Type name (`"Mux"`, `"Connection"`)
   - Handle name (`"mux1"`, `"conn2"`)
   - Placeholder flag
   - Optional: serialized state if type provides `serialize()` method

2. Store in snapshot:
```json
{
  "foreignTypes": [
    {
      "typeName": "Mux",
      "instances": [
        {
          "handleName": "mux1",
          "placeholder": true,
          "state": null
        }
      ]
    }
  ]
}
```

#### Load Phase

1. Restore namespace hierarchy, procedures, variables
2. For each foreign instance:
   - Check if type is registered (`ForeignRegistry.types["Mux"]`)
   - If **type exists**: Create placeholder command that errors with helpful message
   - If **type has constructor**: Optionally auto-reconstruct with `new()`
   - If **type has `deserialize(state)`**: Call it to restore

3. Create command `mux1` that returns:
```
error: foreign object 'mux1' of type 'Mux' needs reconstruction
hint: call [Mux resurrect mux1 <args>] to recreate this object
```

#### Type Registration with Serialization Support

**Go API**:
```go
feather.RegisterType[*http.ServeMux]("Mux", feather.TypeOptions{
    New: func() *http.ServeMux { return http.NewServeMux() },

    // Optional: serialize instance state
    Serialize: func(mux *http.ServeMux) ([]byte, error) {
        // Custom serialization logic
        return json.Marshal(extractRoutes(mux))
    },

    // Optional: deserialize instance state
    Deserialize: func(data []byte) (*http.ServeMux, error) {
        mux := http.NewServeMux()
        routes := []Route{}
        json.Unmarshal(data, &routes)
        for _, r := range routes {
            mux.HandleFunc(r.Pattern, /* reconstruct handler */)
        }
        return mux, nil
    },

    Methods: /* ... */,
})
```

**JavaScript API**:
```javascript
feather.registerType('Server', {
    new: () => http.createServer(),

    // Optional serialization
    serialize: (server) => {
        return JSON.stringify({ /* server config */ });
    },

    deserialize: (state) => {
        const config = JSON.parse(state);
        const server = http.createServer();
        // Reconfigure server from state
        return server;
    },

    methods: { /* ... */ }
});
```

#### TCL API for Resurrection

```tcl
# After loading state, resurrect foreign objects:
set mux [Mux resurrect mux1]  ;# Uses Deserialize if available
# OR
set mux [Mux new]             ;# Create fresh instance
rename mux1 ""                ;# Remove placeholder
rename $mux mux1              ;# Use original handle name
```

### Option 2: Registration Manifest

**Approach**: Require applications to declare all foreign types up-front.

```tcl
# At application start (before save OR load)
package require http-extensions
package require db-extensions

# These packages register all foreign types
# Save captures registered type names
# Load validates all types are still registered
```

**Save checks**:
- Record all registered type names in snapshot
- Error if type not registered when saving instance

**Load checks**:
- Verify all types from snapshot are registered
- Error if any type missing: `"Cannot load state: foreign type 'Mux' not registered"`

**Trade-offs**:
- ✅ Explicit contract - no surprises
- ✅ Works well with package systems
- ❌ Rigid - can't load state if new version removed a type
- ❌ Doesn't help with instance-specific state

### Option 3: Foreign Object Store Hook

**Approach**: Let host provide custom persistence callbacks.

**API Extension**:
```go
type ForeignPersistence interface {
    // Called during save
    SaveForeignState(typeName string, instances []ForeignInstance) ([]byte, error)

    // Called during load
    LoadForeignState(typeName string, data []byte) ([]ForeignInstance, error)
}

interp.SetForeignPersistence(myPersistenceHandler)
```

**Host implements custom logic**:
```go
func (h *MyApp) SaveForeignState(typeName string, instances []ForeignInstance) ([]byte, error) {
    switch typeName {
    case "Mux":
        return h.saveMuxState(instances)
    case "Connection":
        return h.saveConnectionState(instances)
    default:
        return nil, fmt.Errorf("cannot save type %s", typeName)
    }
}
```

**Trade-offs**:
- ✅ Maximum flexibility
- ✅ Host controls serialization format
- ❌ Complex API
- ❌ Host must implement for every type

## Implementation Plan

### Phase 1: Core Persistence API

#### New Commands

**`save_state` builtin**:
```tcl
save_state <filename> <resume_script>
```

- Captures interpreter state to JSON file
- Stores resume target: script to run after load
- Returns handle to saved state file

**`load_state` builtin**:
```tcl
load_state <filename>
```

- Restores interpreter state from JSON
- Recreates namespaces, procedures, variables
- Registers placeholder commands for foreign objects
- Returns resume script path

**`save_and_quit` command** (high-level):
```tcl
proc save_and_quit {resume_script} {
    save_state ~/.feather_state.json $resume_script
    exit 0
}
```

#### Go Implementation

**Files to modify**:
- `feather.go`: Add `SaveState()`, `LoadState()` methods
- `interp_persist.go`: NEW - serialization logic
- `src/builtin_save.c`: NEW - C bindings for save_state
- `src/builtin_load.c`: NEW - C bindings for load_state

**Key functions**:
```go
func (i *Interp) SaveState(filename string, resumeScript string) error {
    snapshot := &StateSnapshot{
        Version:        "1.0",
        FeatherVersion: Version,
        ResumeTarget:   resumeScript,
        Namespaces:     i.serializeNamespaces(),
        Procedures:     i.serializeProcedures(),
        ForeignTypes:   i.serializeForeignTypes(),
        Config:         i.serializeConfig(),
    }

    data, err := json.MarshalIndent(snapshot, "", "  ")
    if err != nil { return err }

    return os.WriteFile(filename, data, 0644)
}

func (i *Interp) LoadState(filename string) (string, error) {
    data, err := os.ReadFile(filename)
    if err != nil { return "", err }

    var snapshot StateSnapshot
    if err := json.Unmarshal(data, &snapshot); err != nil {
        return "", err
    }

    // Validate version compatibility
    if snapshot.Version != "1.0" {
        return "", fmt.Errorf("incompatible state version: %s", snapshot.Version)
    }

    // Restore state
    i.restoreNamespaces(snapshot.Namespaces)
    i.restoreProcedures(snapshot.Procedures)
    i.restoreForeignPlaceholders(snapshot.ForeignTypes)
    i.restoreConfig(snapshot.Config)

    return snapshot.ResumeTarget, nil
}
```

#### JavaScript Implementation

**Files to modify**:
- `js/feather.js`: Add `saveState()`, `loadState()` to API
- `js/persist.js`: NEW - serialization helpers

**Key additions**:
```javascript
function saveState(interpId, filename, resumeScript) {
    const interp = interpreters.get(interpId);
    const snapshot = {
        version: '1.0',
        resumeTarget: resumeScript,
        namespaces: serializeNamespaces(interp),
        procedures: serializeProcedures(interp),
        foreignTypes: serializeForeignTypes(interp),
    };

    // In Node.js
    if (typeof require !== 'undefined') {
        const fs = require('fs');
        fs.writeFileSync(filename, JSON.stringify(snapshot, null, 2));
    }
    // In browser, return snapshot for app to handle
    return snapshot;
}
```

### Phase 2: Foreign Object Resurrection

#### Type Registration Extensions

**Go**:
```go
type TypeOptions struct {
    New         func() any
    Methods     map[string]any
    Serialize   func(any) ([]byte, error)   // NEW
    Deserialize func([]byte) (any, error)   // NEW
    StringRep   func(any) string
    Destroy     func(any)
}

func (i *Interp) RegisterType(name string, opts TypeOptions) {
    info := &foreignTypeInfo{
        name:         name,
        newFunc:      reflect.ValueOf(opts.New),
        methods:      /* wrap methods */,
        serialize:    reflect.ValueOf(opts.Serialize),    // NEW
        deserialize:  reflect.ValueOf(opts.Deserialize),  // NEW
        stringRep:    reflect.ValueOf(opts.StringRep),
        destroy:      reflect.ValueOf(opts.Destroy),
    }
    i.ForeignRegistry.types[name] = info
}
```

**JavaScript**:
```javascript
function registerType(interpId, typeName, typeDef) {
    const interp = interpreters.get(interpId);
    interp.foreignTypes.set(typeName, {
        new: typeDef.new,
        methods: typeDef.methods || {},
        serialize: typeDef.serialize,      // NEW
        deserialize: typeDef.deserialize,  // NEW
    });
}
```

#### Placeholder Commands

When loading state with foreign objects that have no `Deserialize`:

```go
func (i *Interp) createForeignPlaceholder(handleName, typeName string) {
    i.RegisterCommand(handleName, func(i *Interp, cmd FeatherObj, args []FeatherObj) FeatherResult {
        i.SetErrorString(fmt.Sprintf(
            "foreign object '%s' of type '%s' needs reconstruction\n" +
            "hint: use [%s resurrect %s ...] or [%s new]",
            handleName, typeName, typeName, handleName, typeName,
        ))
        return ResultError
    })
}
```

#### Resurrection Command

Add `resurrect` subcommand to foreign type constructors:

```tcl
# User manually recreates object with original handle name
set mux1 [Mux new]
$mux1 handle "/" serveIndex
# State references to mux1 now work
```

### Phase 3: TCL Resume Script Support

**Workflow**:
```tcl
# Long-running script
set progress 0
foreach item $bigList {
    incr progress
    process $item

    # Checkpoint every 100 items
    if {$progress % 100 == 0} {
        save_and_quit [list resume_processing $progress]
    }
}

# Resume handler (defined in same file or sourced)
proc resume_processing {progress} {
    global bigList
    foreach item [lrange $bigList $progress end] {
        process $item
    }
}
```

**Boot script** (run by host):
```tcl
# Check for saved state
if {[file exists ~/.feather_state.json]} {
    set resume_script [load_state ~/.feather_state.json]
    eval $resume_script
    file delete ~/.feather_state.json
} else {
    # Normal startup
    source main.tcl
}
```

## Testing Strategy

### Unit Tests

1. **Serialize/deserialize primitives**:
   - Strings, ints, doubles, lists, dicts
   - Nested structures
   - Unicode handling

2. **Namespace persistence**:
   - Hierarchy preservation
   - Variable values
   - Procedure definitions

3. **Foreign object placeholders**:
   - Placeholder creation on load
   - Error messages
   - Manual resurrection

4. **State versioning**:
   - Reject incompatible versions
   - Handle missing fields gracefully

### Integration Tests

1. **Save and load cycle**:
   ```tcl
   set x 42
   proc double {n} { expr {$n * 2} }
   save_state test.json ""
   # ... new interpreter ...
   load_state test.json
   assert {$x == 42}
   assert {[double 5] == 10}
   ```

2. **Foreign object handling**:
   ```tcl
   set mux [Mux new]
   save_state test.json ""
   # ... new interpreter ...
   load_state test.json
   catch {$mux handle "/" foo} err
   assert {[string match "*needs reconstruction*" $err]}
   ```

3. **Resume script execution**:
   ```tcl
   save_state test.json "puts RESUMED"
   # ... new interpreter ...
   set script [load_state test.json]
   eval $script  ;# Prints "RESUMED"
   ```

### Cross-Host Compatibility

Test that Go-saved states can be loaded by JS (and vice versa):
- Save with Go host
- Load with JS host
- Verify namespace vars, procedures work
- Document foreign type incompatibilities

## Security Considerations

### Deserialization Risks

**Threat**: Malicious state files could exploit deserializer.

**Mitigations**:
1. **Version validation** - reject unknown versions
2. **Schema validation** - validate JSON structure
3. **Size limits** - cap snapshot file size (e.g., 100MB)
4. **Sandboxing** - load state in restricted interpreter first
5. **Signature verification** - optionally sign snapshots

**Implementation**:
```go
func (i *Interp) LoadState(filename string) (string, error) {
    // Check file size
    stat, err := os.Stat(filename)
    if err != nil { return "", err }
    if stat.Size() > 100*1024*1024 {
        return "", fmt.Errorf("state file too large: %d bytes", stat.Size())
    }

    // Validate schema
    var snapshot StateSnapshot
    if err := json.Unmarshal(data, &snapshot); err != nil {
        return "", fmt.Errorf("invalid state format: %w", err)
    }

    if snapshot.Version != "1.0" {
        return "", fmt.Errorf("unsupported version: %s", snapshot.Version)
    }

    // ... restore state ...
}
```

### Resume Script Injection

**Threat**: Attacker modifies state file to inject malicious resume script.

**Mitigations**:
1. **Signature verification** - sign `resumeTarget` field
2. **Allowlist** - only allow pre-approved resume scripts
3. **User confirmation** - prompt before running resume script

**Implementation**:
```tcl
# Allowlist approach
set allowed_resume_scripts {
    resume_processing
    continue_server
    restart_job
}

proc load_and_resume {statefile} {
    set resume [load_state $statefile]
    set proc_name [lindex $resume 0]

    if {$proc_name ni $allowed_resume_scripts} {
        error "Untrusted resume script: $proc_name"
    }

    eval $resume
}
```

## Feasibility Assessment

### Go Host: ✅ HIGHLY FEASIBLE

| Component | Feasibility | Notes |
|-----------|-------------|-------|
| Namespace serialization | ✅ Straightforward | Use `json.Marshal` on `Namespace` struct |
| Procedure serialization | ✅ Straightforward | Procs stored as `*Obj` (strings), easy to serialize |
| Foreign type registry | ✅ Straightforward | Type names + optional Serialize/Deserialize |
| Foreign instances | ⚠️ Partial | Requires Serialize support or manual resurrection |
| Variable links | ✅ Straightforward | Serialize `varLink` struct |
| Cross-version compat | ✅ Manageable | Version field + schema validation |

**Complexity**: Medium (3-5 days for core implementation)

### JavaScript Host: ✅ HIGHLY FEASIBLE

| Component | Feasibility | Notes |
|-----------|-------------|-------|
| Namespace serialization | ✅ Straightforward | Already has `materialize()` for persistence |
| Procedure serialization | ✅ Straightforward | Stored in namespace vars as materialized values |
| Foreign type registry | ✅ Straightforward | Type names + optional serialize/deserialize |
| Foreign instances | ⚠️ Partial | Requires serialize support or manual resurrection |
| Browser vs Node.js | ⚠️ Split | Node.js: filesystem; Browser: localStorage/IndexedDB |
| Cross-version compat | ✅ Manageable | Version field + schema validation |

**Complexity**: Medium (3-5 days for core implementation)

**Browser-specific notes**:
- Use `localStorage` for small states (<5MB)
- Use `IndexedDB` for large states
- Provide export/import API for user-managed persistence

### Cross-Host Compatibility: ⚠️ PARTIAL

| Aspect | Compatibility | Notes |
|--------|---------------|-------|
| TCL state (vars, procs) | ✅ Full | JSON is portable |
| Foreign objects | ❌ None | Go and JS types are incompatible |
| State format | ✅ Full | JSON with version field |

**Recommendation**: Document that foreign objects are host-specific.

## Recommendations

### Minimum Viable Implementation

**Priority 1** (must-have):
1. `save_state` / `load_state` builtins
2. Namespace + procedure serialization
3. Foreign object placeholders with error messages
4. Basic versioning

**Priority 2** (should-have):
1. `Serialize` / `Deserialize` optional callbacks for foreign types
2. Resume script support
3. Schema validation

**Priority 3** (nice-to-have):
1. State file signatures
2. Cross-host compatibility testing
3. Browser localStorage support (JS)
4. Incremental/checkpoint saves

### API Design

**Recommended API** (balance simplicity + flexibility):

```tcl
# Basic save/load
save_state filename.json ?resume_script?
load_state filename.json → returns resume_script

# High-level wrapper
save_and_quit resume_script  ;# Saves + exits

# Type registration (Go)
interp.RegisterType("Mux", TypeOptions{
    New: func() *http.ServeMux { return http.NewServeMux() },
    Serialize: func(m *http.ServeMux) ([]byte, error) { /* custom */ },
    Deserialize: func(data []byte) (*http.ServeMux, error) { /* custom */ },
    Methods: map[string]any{
        "listen": func(m *http.ServeMux, port int) { /* ... */ },
    },
})

# Type registration (JS)
feather.registerType('Server', {
    new: () => http.createServer(),
    serialize: (s) => JSON.stringify({/* state */}),
    deserialize: (data) => { const s = http.createServer(); /* restore */; return s; },
    methods: { /* ... */ },
})
```

### Documentation Requirements

1. **User guide**: How to use `save_state` / `load_state`
2. **Foreign type guide**: How to implement `Serialize` / `Deserialize`
3. **Migration guide**: Handling version upgrades
4. **Security guide**: Validating state files
5. **Examples**: Checkpointing long-running scripts, session restoration

## Conclusion

**Interpreter state persistence is feasible** for both Go and JavaScript hosts with the following approach:

1. ✅ **TCL state** (namespaces, procedures, variables) - fully serializable
2. ⚠️ **Foreign objects** - require opt-in `Serialize`/`Deserialize` or manual resurrection
3. ✅ **Resume scripts** - store target script, eval after load
4. ✅ **Versioning** - schema validation prevents incompatibilities

**Recommended strategy**:
- Start with **Option 1** (placeholders + manual resurrection)
- Add optional `Serialize`/`Deserialize` for types that need it
- Provide clear error messages guiding users to resurrect foreign objects
- Document that foreign objects are host-specific (no cross-host support)

**Timeline estimate**: 1-2 weeks for MVP (both hosts)

**Files to create/modify**: ~8-12 files total (C builtins, Go/JS serialization, tests, docs)
