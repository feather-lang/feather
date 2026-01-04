# Command Replay Approach to Foreign Object Persistence

## Core Insight

Instead of serializing foreign object **state**, serialize the **commands** that created and configured the object. Then replay those commands on restore.

This leverages TCL's fundamental nature: **everything is a command**.

## Example

### Original Session
```tcl
set mux [Mux new]
$mux handle "/api" apiHandler
$mux handle "/static" staticHandler
set server [Server new]
$server setMux $mux
```

### Saved State (JSON)
```json
{
  "commandLog": [
    {
      "id": "mux_1",
      "variable": "mux",
      "commands": [
        {"cmd": "Mux", "args": ["new"], "result": "$mux_handle"},
        {"cmd": "$mux_handle", "args": ["handle", "/api", "apiHandler"]},
        {"cmd": "$mux_handle", "args": ["handle", "/static", "staticHandler"]}
      ]
    },
    {
      "id": "server_1",
      "variable": "server",
      "commands": [
        {"cmd": "Server", "args": ["new"], "result": "$server_handle"},
        {"cmd": "$server_handle", "args": ["setMux", "@mux_1"]}
      ]
    }
  ]
}
```

### Restore Session
```tcl
# Replay commands in order
set mux [Mux new]
$mux handle "/api" apiHandler
$mux handle "/static" staticHandler
set server [Server new]
$server setMux $mux
```

## Advantages

### 1. **No Custom Serialization Required**
- Works for any foreign type automatically
- No need for type-specific Serialize/Deserialize methods
- Host doesn't need to understand internal object structure

### 2. **Leverages TCL's Nature**
- Commands are already the canonical representation
- What you type is what gets saved
- Debugging is trivial (just read the command log)

### 3. **Human-Readable State**
- Can manually edit saved state
- Can see exactly how objects were configured
- Easy to diff between saves

### 4. **Handles Complex Configuration**
- Multi-step setup is captured naturally
- Order of operations preserved
- Method chaining works

### 5. **Type Versioning**
- If new version adds optional parameters, old command logs still work
- Graceful degradation

## Implementation Approaches

### Approach 1: Explicit Recording Mode

**API**:
```tcl
# Start recording
record_start

set mux [Mux new]
$mux handle "/" index
$mux listen 8080

# Stop recording and save
record_stop
save_state checkpoint.json "resume_server"
```

**Pros**:
- Explicit control over what's recorded
- No performance overhead when not recording
- Clear boundaries

**Cons**:
- Requires user to remember to enable recording
- Easy to forget
- Manual workflow

### Approach 2: Automatic Command Tracing

**Implementation**:
```tcl
# Automatically trace all foreign object commands
# When save_state is called, replay log is extracted
set mux [Mux new]  # Internally logged
$mux handle "/" index  # Internally logged

save_state checkpoint.json "resume"
```

**Mechanism** (Go):
```go
type commandLogEntry struct {
    timestamp time.Time
    command   string
    args      []string
    result    string
}

// In foreignMethodDispatch:
func (i *Interp) foreignMethodDispatch(handleName string, cmd FeatherObj, args []FeatherObj) FeatherResult {
    // Log the command before executing
    if i.recordingEnabled {
        entry := commandLogEntry{
            timestamp: time.Now(),
            command:   handleName,
            args:      i.getArgStrings(args),
        }
        i.commandLog = append(i.commandLog, entry)
    }

    // Execute normally...
    result := i.callForeignMethod(...)

    // Log the result
    if i.recordingEnabled && result == ResultOK {
        entry.result = i.getString(i.result)
    }

    return result
}
```

**Pros**:
- Automatic - no user intervention
- Captures all foreign object interactions
- Transparent

**Cons**:
- Memory overhead (unbounded log)
- May capture side-effect commands (like `listen`)
- Need to distinguish config vs. activation

### Approach 3: Declarative Object Construction

**Force users to create objects declaratively**:
```tcl
# Instead of imperative:
set mux [Mux new]
$mux handle "/" index

# Require declarative:
set mux [Mux new {
    handle "/" index
    handle "/api" apiHandler
}]
```

**Pros**:
- State is inherently serializable (it's already a script)
- Clean separation of construction vs. activation
- Self-documenting

**Cons**:
- Changes API significantly
- Not compatible with existing foreign object patterns
- Requires type implementers to support this pattern

## Challenges

### Challenge 1: Non-Serializable Arguments

**Problem**:
```tcl
set handler [Handler new]
$mux handle "/" $handler
```

The `$handler` is itself a foreign object. Can't serialize the handle name.

**Solution**: Build dependency graph
```json
{
  "objects": {
    "handler_1": {
      "commands": [["Handler", "new"]]
    },
    "mux_1": {
      "commands": [
        ["Mux", "new"],
        ["$mux_handle", "handle", "/", "@handler_1"]  // Reference by ID
      ]
    }
  }
}
```

On restore:
1. Topologically sort objects by dependencies
2. Create `handler_1` first
3. Create `mux_1`, substituting `@handler_1` with actual handle

### Challenge 2: Side Effects

**Problem**:
```tcl
$server listen 8080  # Binds to port!
```

Some commands have external effects. Replaying blindly could:
- Fail (port already in use)
- Have unintended effects (send emails, modify files)
- Be dangerous (delete data)

**Solutions**:

**A. Lifecycle Phases** - Separate configuration from activation:
```tcl
# Configuration phase (safe to replay)
$server setPort 8080
$server setHandler $mux

# Activation phase (NOT saved)
$server start  # Only called in resume script, not logged
```

**Convention**: Methods like `start`, `listen`, `run`, `execute` are "activators" and not logged.

**B. Idempotent Commands** - Type implementers ensure commands can be replayed:
```go
// Instead of:
func (s *Server) listen(port int) error {
    return s.httpServer.Listen(port)  // Fails if already listening
}

// Make idempotent:
func (s *Server) listen(port int) error {
    if s.isListening {
        s.stop()
    }
    return s.httpServer.Listen(port)
}
```

**C. Dry-Run Mode** - Restore in paused state:
```tcl
load_state checkpoint.json
# Objects created but not activated
# User manually calls activation:
$server start
```

### Challenge 3: Query Results

**Problem**:
```tcl
set results [$db query "SELECT * FROM users"]
foreach row $results {
    # process
}
```

The `$results` foreign object was created as a **result** of a command, not directly instantiated.

**Solutions**:

**A. Don't Persist Ephemeral Objects** - Mark certain foreign types as non-persistent:
```go
RegisterType("QueryResult", TypeOptions{
    Ephemeral: true,  // Don't save these
})
```

On save, error if ephemeral foreign objects are in global scope:
```
Error: Cannot save state with ephemeral object 'results' of type 'QueryResult'
Hint: Store only the query, not the results
```

**B. Store Constructor Expression**:
```json
{
  "results_1": {
    "type": "QueryResult",
    "constructor": ["$db", "query", "SELECT * FROM users"]
  }
}
```

On restore, re-execute the query.

**Trade-off**: Results may differ (data changed between sessions).

### Challenge 4: Circular References

**Problem**:
```tcl
set a [Foo new]
set b [Bar new]
$a setBar $b
$b setFoo $a
```

**Solution**: Topological sort with placeholders
```json
{
  "a_1": {
    "commands": [
      ["Foo", "new"],
      ["$a_handle", "setBar", "@b_1"]  // Forward reference
    ]
  },
  "b_1": {
    "commands": [
      ["Bar", "new"],
      ["$b_handle", "setFoo", "@a_1"]  // Back reference
    ]
  }
}
```

Restore algorithm:
1. Create all objects first (call constructors)
2. Execute configuration commands in declaration order
3. Resolve `@id` references to actual handles

### Challenge 5: Command Results Used in Computation

**Problem**:
```tcl
set count [$db executeScalar "SELECT COUNT(*) FROM users"]
set limit [expr {$count * 2}]
$query setLimit $limit
```

The `$limit` depends on a foreign object method result.

**Solutions**:

**A. Only Log Final Values**:
```json
{
  "query_1": {
    "commands": [
      ["Query", "new"],
      ["$query_handle", "setLimit", "246"]  // Just the value, not the computation
    ]
  }
}
```

**Pro**: Simple, works if value is what matters
**Con**: Loses the computation logic (if data changed, limit should change)

**B. Store Full Expression** (Advanced):
```json
{
  "expressions": [
    {"var": "count", "expr": ["$db", "executeScalar", "SELECT COUNT(*) FROM users"]},
    {"var": "limit", "expr": ["expr", {"ref": "count"}, "*", "2"]}
  ]
}
```

**Pro**: Preserves computation logic
**Con**: Very complex, essentially serializing TCL AST

## Recommended Hybrid Approach

### Core Design

**1. Automatic Command Logging for Foreign Objects**
- When foreign instance command is executed, log it
- Only log if result is `ResultOK`
- Log includes: timestamp, handle name, method, args, result

**2. Smart Filtering**
- Don't log "activator" methods (configurable blacklist: `start`, `listen`, `run`, `activate`)
- Don't log ephemeral types (types marked `Ephemeral: true`)
- Don't log if arguments contain ephemeral objects

**3. Dependency Tracking**
- When foreign object is passed as argument, replace with `@objectId`
- Build dependency graph
- Topologically sort on save

**4. Restore Process**
```tcl
load_state checkpoint.json
# 1. Create all foreign objects (constructors)
# 2. Execute configuration commands (dependency order)
# 3. Set regular variables
# 4. Return resume script
```

**5. User Control**
```tcl
# Opt-in to logging for specific objects
record_object $mux

# Or opt-in globally
record_all_foreign

# Exclude specific methods
record_exclude_method "listen"
record_exclude_method "start"
```

## API Design

### Type Registration

```go
RegisterType("Server", TypeOptions{
    New: func() *Server { return &Server{} },
    Methods: map[string]any{
        "setPort": func(s *Server, port int) { s.port = port },
        "setMux":  func(s *Server, mux *http.ServeMux) { s.mux = mux },
        "start":   func(s *Server) error { return s.httpServer.ListenAndServe() },
    },

    // NEW: Mark activation methods
    ActivationMethods: []string{"start", "listen"},

    // NEW: Mark as ephemeral (don't persist)
    Ephemeral: false,
})
```

### TCL API

```tcl
# Automatic mode (default)
set mux [Mux new]
$mux handle "/" index  # Automatically logged
save_state checkpoint.json "resume"

# Explicit mode
record_start
# ... commands ...
record_stop
save_state checkpoint.json "resume"

# Exclude specific activations
record_exclude_method "listen"
$server listen 8080  # Not logged

# Load and replay
load_state checkpoint.json
# Foreign objects recreated via command replay
eval [load_state checkpoint.json]
```

## State File Format

```json
{
  "version": "1.0",
  "timestamp": "2026-01-04T15:30:00Z",
  "resumeTarget": "resume_server",

  "namespaces": {
    "::": {
      "vars": {
        "port": {"type": "int", "value": 8080}
      }
    }
  },

  "procedures": {
    "apiHandler": {
      "params": ["req", "res"],
      "body": "..."
    }
  },

  "foreignObjects": {
    "mux_1": {
      "type": "Mux",
      "handleName": "mux1",
      "variable": "mux",
      "commands": [
        {
          "seq": 1,
          "cmd": "Mux",
          "args": ["new"],
          "result": "mux1"
        },
        {
          "seq": 2,
          "cmd": "mux1",
          "args": ["handle", "/", "index"]
        },
        {
          "seq": 3,
          "cmd": "mux1",
          "args": ["handle", "/api", "apiHandler"]
        }
      ],
      "dependencies": []
    },

    "server_1": {
      "type": "Server",
      "handleName": "server1",
      "variable": "server",
      "commands": [
        {
          "seq": 4,
          "cmd": "Server",
          "args": ["new"],
          "result": "server1"
        },
        {
          "seq": 5,
          "cmd": "server1",
          "args": ["setPort", "8080"]
        },
        {
          "seq": 6,
          "cmd": "server1",
          "args": ["setMux", "@mux_1"]
        }
      ],
      "dependencies": ["mux_1"]
    }
  }
}
```

## Implementation Complexity

### Phase 1: Basic Command Logging
- Add command log to `Interp` struct
- Hook into `foreignMethodDispatch` to record commands
- Serialize command log to JSON
- Restore by replaying commands
- **Complexity**: Medium (4-6 days)

### Phase 2: Dependency Tracking
- Detect foreign object arguments
- Build dependency graph
- Topological sort on save
- Resolve references on load
- **Complexity**: Medium-High (5-7 days)

### Phase 3: Smart Filtering
- Activation method blacklist
- Ephemeral type support
- User-controlled recording
- **Complexity**: Low-Medium (2-3 days)

**Total**: 2-3 weeks for complete implementation

## Comparison with Original Approach

| Aspect | Serialize State | Command Replay |
|--------|----------------|----------------|
| **Implementation burden** | Type implementer writes Serialize/Deserialize | Automatic, no extra code |
| **Debuggability** | Binary/JSON blob, opaque | Human-readable commands |
| **Flexibility** | Handles internal state | Handles configuration only |
| **Side effects** | N/A (state is snapshot) | Must be careful with activators |
| **Accuracy** | Perfect state replication | Depends on idempotent commands |
| **Type versioning** | Must handle schema changes | Commands may work across versions |
| **Ephemeral data** | Can serialize query results | Can't serialize, must re-query |
| **Circular refs** | Easy (direct pointers) | Need dependency tracking |

## Recommended Hybrid: Best of Both Worlds

**Combine both approaches**:

```go
RegisterType("Database", TypeOptions{
    // Option 1: Provide Serialize (preferred for stateful objects)
    Serialize: func(db *DB) ([]byte, error) {
        return json.Marshal(db.connectionString)
    },
    Deserialize: func(data []byte) (*DB, error) {
        connStr := string(data)
        return sql.Open("postgres", connStr)
    },

    // Option 2: Fall back to command replay (automatic)
    // If Serialize not provided, use command log
})
```

**Restore logic**:
1. If type has `Deserialize`, use it
2. Else, replay command log
3. If neither available, create placeholder

**Best of both**:
- ✅ Stateful objects can serialize efficiently
- ✅ Stateless objects work automatically via command replay
- ✅ No objects left behind (fallback to placeholder)

## Conclusion

**Command replay is viable and elegant** for configuration-style foreign objects:
- HTTP routers (routes are commands)
- Template engines (template registration is commands)
- Loggers (configuration is commands)

**But challenges remain**:
- Side effects need careful handling
- Ephemeral objects (query results) can't be replayed
- Circular references need dependency tracking

**Recommendation**:
Start with **automatic command logging** for all foreign objects, with smart filtering for activators. This gives a solid foundation. Then add **optional Serialize/Deserialize** for types where command replay isn't sufficient (database connections, network sockets, etc.).

This gives users the best of both worlds:
- Simple configuration objects: work automatically via command replay
- Complex stateful objects: opt-in to custom serialization
