// Package feather provides an embeddable TCL interpreter for Go applications.
//
// # Architecture
//
// feather has a layered architecture:
//
//   - C core: The parsing and evaluation engine written in C
//   - Handle layer: Internal numeric handles (FeatherObj) for C interop
//   - Obj layer: The public Go API using [*Obj] values
//
// As a user of this package, you work exclusively with [*Obj] values.
// The Handle types exist only for internal implementation and may change
// between versions.
//
// # Quick Start
//
//	interp := feather.New()
//	defer interp.Close()
//
//	// Evaluate TCL scripts
//	result, err := interp.Eval("expr {2 + 2}")
//	if err != nil {
//	    log.Fatal(err)
//	}
//	fmt.Println(result.String()) // "4"
//
//	// Register Go functions as TCL commands
//	interp.Register("env", func(name string) string {
//	    return os.Getenv(name)
//	})
//
//	result, _ = interp.Eval(`env HOME`)
//	fmt.Println(result.String()) // "/home/user"
//
// # Thread Safety
//
// An [*Interp] is NOT safe for concurrent use from multiple goroutines.
// Each goroutine that needs to evaluate TCL must have its own interpreter:
//
//	// WRONG: sharing interpreter between goroutines
//	interp := feather.New()
//	go func() { interp.Eval("...") }() // data race!
//	go func() { interp.Eval("...") }() // data race!
//
//	// CORRECT: one interpreter per goroutine
//	go func() {
//	    interp := feather.New()
//	    defer interp.Close()
//	    interp.Eval("...")
//	}()
//
// For server applications, use a pool of interpreters or create one per request.
// [*Obj] values are also tied to their interpreter and must not be shared.
//
// # Supported TCL Commands
//
// feather implements a substantial subset of TCL 8.6. Available commands:
//
// Control flow:
//
//	if, while, for, foreach, switch, break, continue, return, tailcall
//
// Procedures and evaluation:
//
//	proc, apply, eval, uplevel, upvar, catch, try, throw, error
//
// Variables and namespaces:
//
//	set, unset, incr, append, global, variable, namespace, rename, trace
//
// Lists:
//
//	list, llength, lindex, lrange, lappend, lset, linsert, lreplace,
//	lreverse, lrepeat, lsort, lsearch, lmap, lassign, split, join, concat
//
// Dictionaries:
//
//	dict (with subcommands: create, get, set, exists, keys, values, etc.)
//
// Strings:
//
//	string (with subcommands: length, index, range, equal, compare,
//	        match, map, tolower, toupper, trim, replace, first, last, etc.)
//	format, scan, subst
//
// Introspection:
//
//	info (with subcommands: exists, commands, procs, vars, body, args,
//	      level, frame, script, etc.)
//
// Math functions (via expr):
//
//	sqrt, exp, log, log10, sin, cos, tan, asin, acos, atan, atan2,
//	sinh, cosh, tanh, floor, ceil, round, abs, pow, fmod, hypot,
//	double, int, wide, isnan, isinf
//
// NOT implemented: file I/O, sockets, regex, clock, encoding, interp (safe interps),
// and most Tk-related commands. Use [Interp.Register] to add these if needed.
//
// # Error Handling
//
// Errors from [Interp.Eval] are returned as [*EvalError]:
//
//	result, err := interp.Eval("expr {1/0}")
//	if err != nil {
//	    // err is *EvalError, err.Error() returns the message
//	    fmt.Println("Error:", err)
//	}
//
// To return errors from Go commands, use [Error] or [Errorf]:
//
//	interp.RegisterCommand("fail", func(i *feather.Interp, cmd *feather.Obj, args []*feather.Obj) feather.Result {
//	    // For Go errors, use err.Error() to get the string
//	    _, err := os.Open("/nonexistent")
//	    if err != nil {
//	        return feather.Error(err.Error())
//	    }
//	    return feather.OK("success")
//	})
//
// For functions registered with [Interp.Register], return an error as the last value:
//
//	interp.Register("openfile", func(path string) (string, error) {
//	    data, err := os.ReadFile(path)
//	    return string(data), err  // error automatically becomes TCL error
//	})
//
// In TCL, use catch or try to handle errors:
//
//	if {[catch {openfile /nonexistent} errmsg]} {
//	    puts "Error: $errmsg"
//	}
//
// Note: feather does not currently provide stack traces or line numbers in errors.
// The error message is the only diagnostic information available.
//
// # Working with Results
//
// [Interp.Eval] returns (*Obj, error). The result is the value of the last
// command executed. Extract values using methods on [*Obj] or the As* functions:
//
//	result, _ := interp.Eval("expr {2 + 2}")
//
//	// As string (always works)
//	s := result.String()  // "4"
//
//	// As typed values (may error if not convertible)
//	n, err := result.Int()       // 4, nil
//	f, err := result.Double()    // 4.0, nil
//	b, err := result.Bool()      // true, nil
//
//	// For lists, first check if it's already a list or parse it
//	result, _ = interp.Eval("list a b c")
//	items, err := result.List()  // []*Obj{"a", "b", "c"}
//	// Or parse a string as a list:
//	items, err = interp.ParseList("a b {c d}")
//
// The [Result] type is only used when implementing commands with [Interp.RegisterCommand].
// Create results with [OK], [Error], or [Errorf].
//
// # Memory and Lifetime
//
// [*Obj] values are managed by Go's garbage collector. You don't need to
// explicitly free them. However:
//
//   - After [Interp.Close], all [*Obj] values from that interpreter become invalid
//   - Don't store [*Obj] values beyond the interpreter's lifetime
//   - Don't share [*Obj] values between interpreters
//
// For long-lived applications, be aware that string representations are cached.
// An object that shimmers between int and string keeps both representations
// until garbage collected.
//
// # The Obj Type System
//
// TCL values are represented by [*Obj]. Each Obj has two representations:
//
//   - String representation: The TCL string form (always available)
//   - Internal representation: An efficient native form (optional)
//
// The internal representation is managed through the [ObjType] interface.
// Conversion between representations happens lazily through "shimmering":
// when you request a value as an integer, it parses the string and caches
// the int; when you later request the string, it's regenerated from the int.
//
// Use the As* functions to convert between types:
//
//	n, err := feather.AsInt(obj)      // Get as int64
//	f, err := feather.AsDouble(obj)   // Get as float64
//	b, err := feather.AsBool(obj)     // Get as bool
//	list, err := feather.AsList(obj)  // Get as []*Obj (requires list rep)
//	dict, err := feather.AsDict(obj)  // Get as *DictType (requires dict rep)
//
// Note: AsList and AsDict only work on objects that already have list/dict
// representations. To parse a string as a list or dict, use the interpreter:
//
//	list, err := interp.ParseList("a b {c d}")   // Parse string to list
//	dict, err := interp.ParseDict("name Alice")  // Parse string to dict
//
// # Custom Object Types
//
// Implement [ObjType] to create types that participate in shimmering.
// This is useful when you have a Go type that's expensive to create from
// its string form, so you want to cache the parsed representation.
//
// The interface has three methods:
//
//	type ObjType interface {
//	    Name() string           // Type name for debugging (e.g., "regex")
//	    UpdateString() string   // Convert internal rep back to string
//	    Dup() ObjType           // Clone the internal rep (for Copy)
//	}
//
// Example: A regex type that caches compiled patterns:
//
//	type RegexType struct {
//	    pattern string
//	    re      *regexp.Regexp
//	}
//
//	func (r *RegexType) Name() string         { return "regex" }
//	func (r *RegexType) UpdateString() string { return r.pattern }
//	func (r *RegexType) Dup() feather.ObjType { return r } // Immutable, share it
//
//	func NewRegex(interp *feather.Interp, pattern string) (*feather.Obj, error) {
//	    re, err := regexp.Compile(pattern)
//	    if err != nil {
//	        return nil, err
//	    }
//	    return interp.Obj(&RegexType{pattern: pattern, re: re}), nil
//	}
//
//	// Extract the compiled regex from any Obj
//	func GetRegex(obj *feather.Obj) (*regexp.Regexp, bool) {
//	    if rt, ok := obj.InternalRep().(*RegexType); ok {
//	        return rt.re, true
//	    }
//	    return nil, false
//	}
//
// # Conversion Interfaces
//
// Custom types can implement conversion interfaces to participate in
// automatic type coercion. When [AsInt] is called on an Obj, it first
// checks if the internal representation implements [IntoInt]:
//
//	type IntoInt interface {
//	    IntoInt() (int64, bool)
//	}
//
// If implemented and returns (value, true), that value is used directly
// without parsing the string representation. This enables efficient
// conversions between related types.
//
// Available conversion interfaces:
//
//	IntoInt    - Convert to int64
//	IntoDouble - Convert to float64
//	IntoBool   - Convert to bool
//	IntoList   - Convert to []*Obj
//	IntoDict   - Convert to (map[string]*Obj, []string)
//
// Example: A timestamp type that converts to int (Unix epoch):
//
//	type TimestampType struct {
//	    t time.Time
//	}
//
//	func (ts *TimestampType) Name() string         { return "timestamp" }
//	func (ts *TimestampType) UpdateString() string { return ts.t.Format(time.RFC3339) }
//	func (ts *TimestampType) Dup() feather.ObjType { return ts }
//
//	// Implement IntoInt to support "expr {$timestamp + 3600}"
//	func (ts *TimestampType) IntoInt() (int64, bool) {
//	    return ts.t.Unix(), true
//	}
//
//	// Implement IntoDouble for sub-second precision
//	func (ts *TimestampType) IntoDouble() (float64, bool) {
//	    return float64(ts.t.UnixNano()) / 1e9, true
//	}
//
// # Foreign Objects
//
// For exposing Go structs with methods to TCL, use [RegisterType].
// Unlike [ObjType] (which is about caching parsed representations),
// foreign types create objects that act as TCL commands with methods:
//
//	type DB struct {
//	    conn *sql.DB
//	}
//
//	feather.RegisterType[*DB](interp, "DB", feather.TypeDef[*DB]{
//	    New: func() *DB {
//	        conn, _ := sql.Open("sqlite3", ":memory:")
//	        return &DB{conn: conn}
//	    },
//	    Methods: map[string]any{
//	        "exec":  func(db *DB, sql string) error { _, err := db.conn.Exec(sql); return err },
//	        "query": func(db *DB, sql string) ([]string, error) { /* ... */ },
//	    },
//	    Destroy: func(db *DB) { db.conn.Close() },
//	})
//
//	// In TCL:
//	// set db [DB new]
//	// $db exec "CREATE TABLE users (name TEXT)"
//	// $db destroy
//
// # Registering Commands
//
// For simple functions, use [Interp.Register] with automatic type conversion:
//
//	// String arguments
//	interp.Register("upper", strings.ToUpper)
//
//	// Multiple parameters with error return
//	interp.Register("readfile", func(path string) (string, error) {
//	    data, err := os.ReadFile(path)
//	    return string(data), err
//	})
//
//	// Variadic functions
//	interp.Register("sum", func(nums ...int) int {
//	    total := 0
//	    for _, n := range nums {
//	        total += n
//	    }
//	    return total
//	})
//
// For full control over argument handling, use [Interp.RegisterCommand]:
//
//	interp.RegisterCommand("mycommand", func(i *feather.Interp, cmd *feather.Obj, args []*feather.Obj) feather.Result {
//	    if len(args) < 1 {
//	        return feather.Errorf("usage: %s value", cmd.String())
//	    }
//	    n, err := feather.AsInt(args[0])
//	    if err != nil {
//	        return feather.Error(err.Error())
//	    }
//	    return feather.OK(n * 2)
//	})
//
// # Configuration
//
// Set the recursion limit to prevent stack overflow from deeply nested calls:
//
//	interp.SetRecursionLimit(500)  // Default is 1000
//
// # Parsing Without Evaluation
//
// Use [Interp.Parse] to check if a script is syntactically complete without
// evaluating it. This is useful for implementing REPLs:
//
//	pr := interp.Parse("set x {")
//	switch pr.Status {
//	case feather.ParseOK:
//	    // Complete, ready to evaluate
//	case feather.ParseIncomplete:
//	    // Unclosed brace/bracket/quote, prompt for more input
//	case feather.ParseError:
//	    // Syntax error, pr.Message has details
//	}
//
// # Internal Types (Do Not Use)
//
// The following types are internal implementation details for C interop.
// They are exported only because Go requires it for cgo. Do not use them
// in application code:
//
//   - Handle, FeatherInterp, FeatherObj - Raw numeric handles
//   - InternalCommandFunc - Low-level command signature
//   - InternalParseStatus, ParseResultInternal - Internal parsing types
//   - CallFrame, Namespace, Procedure, Command - Interpreter internals
//   - ForeignRegistry, ForeignType - Interpreter internals
//   - ListSortContext - Internal sorting state
//   - FeatherResult, InternalCommandType - C interop constants
//
// These types may change or be removed in any version.
package feather
