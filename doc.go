// Package feather provides an embeddable TCL interpreter for Go applications.
//
// # Architecture
//
// feather has a layered architecture:
//
//   - C core: The parsing and evaluation engine written in C
//   - Handle layer: Internal numeric handles (FeatherObj, ObjHandle) for C interop
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
//	func NewRegex(pattern string) (*feather.Obj, error) {
//	    re, err := regexp.Compile(pattern)
//	    if err != nil {
//	        return nil, err
//	    }
//	    return feather.NewObj(&RegexType{pattern: pattern, re: re}), nil
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
//	        return feather.Error(err)
//	    }
//	    return feather.OK(n * 2)
//	})
//
// # Internal Types (Do Not Use)
//
// The following types are internal implementation details for C interop.
// They are exported only because Go requires it for cgo. Do not use them
// in application code:
//
//   - Handle, FeatherInterp, FeatherObj - Raw numeric handles
//   - ObjHandle - Handle wrapper with interpreter reference
//   - InternalCommandFunc - Low-level command signature
//   - InternalParseStatus, ParseResultInternal - Internal parsing types
//
// These types may change or be removed in any version.
package feather
