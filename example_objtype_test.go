package feather_test

import (
	"fmt"
	"regexp"
	"time"

	"github.com/feather-lang/feather"
)

// RegexType caches a compiled regular expression.
// This demonstrates the core use case for custom ObjType: avoiding repeated
// parsing of the same value.
type RegexType struct {
	pattern string
	re      *regexp.Regexp
}

func (r *RegexType) Name() string         { return "regex" }
func (r *RegexType) UpdateString() string { return r.pattern }
func (r *RegexType) Dup() feather.ObjType { return r } // Immutable, safe to share

// NewRegexObj compiles a pattern and wraps it in an Obj.
func NewRegexObj(pattern string) (*feather.Obj, error) {
	re, err := regexp.Compile(pattern)
	if err != nil {
		return nil, err
	}
	return feather.NewObj(&RegexType{pattern: pattern, re: re}), nil
}

// GetRegex extracts the compiled regex from an Obj.
// Returns nil, false if the Obj doesn't contain a RegexType.
func GetRegex(obj *feather.Obj) (*regexp.Regexp, bool) {
	if rt, ok := obj.InternalRep().(*RegexType); ok {
		return rt.re, true
	}
	return nil, false
}

// registerRegexCommands adds "regex" and "match" commands to an interpreter.
// Factored out so multiple examples can use it.
func registerRegexCommands(interp *feather.Interp) {
	// Register a command that compiles and caches regex patterns.
	// Returns the compiled regex object on success, or a TCL error on failure.
	interp.RegisterCommand("regex", func(i *feather.Interp, cmd *feather.Obj, args []*feather.Obj) feather.Result {
		if len(args) < 1 {
			return feather.Errorf("wrong # args: should be \"regex pattern\"")
		}
		obj, err := NewRegexObj(args[0].String())
		if err != nil {
			// Return the Go error as a TCL error.
			// Use err.Error() to get the string - passing error directly
			// would quote special characters like [ and ].
			return feather.Error(err.Error())
		}
		return feather.OK(obj)
	})

	// Register a match command that uses the cached regex
	interp.RegisterCommand("match", func(i *feather.Interp, cmd *feather.Obj, args []*feather.Obj) feather.Result {
		if len(args) < 2 {
			return feather.Errorf("wrong # args: should be \"match regex string\"")
		}
		re, ok := GetRegex(args[0])
		if !ok {
			// Not a cached regex - try to compile from string representation
			var err error
			re, err = regexp.Compile(args[0].String())
			if err != nil {
				return feather.Error(err.Error())
			}
		}
		if re.MatchString(args[1].String()) {
			return feather.OK(1)
		}
		return feather.OK(0)
	})
}

// This example shows how to create and use a custom ObjType for caching
// compiled regular expressions.
func Example_regexType() {
	interp := feather.New()
	defer interp.Close()
	registerRegexCommands(interp)

	// Use the regex - the pattern is compiled once and reused
	result, _ := interp.Eval(`
		set re [regex {^\d+$}]
		match $re "12345"
	`)
	fmt.Println(result.String())
	// Output: 1
}

// This example shows how errors from Go are propagated back to TCL.
// When regexp.Compile fails, the error becomes a TCL error that can
// be caught with "catch".
func Example_regexTypeError() {
	interp := feather.New()
	defer interp.Close()
	registerRegexCommands(interp)

	// Invalid regex pattern - repetition operator at start
	_, err := interp.Eval(`regex {*invalid}`)
	fmt.Println("Direct error:", err)

	// Using catch to handle the error in TCL
	result, _ := interp.Eval(`
		if {[catch {regex {+also-invalid}} errmsg]} {
			set errmsg
		} else {
			set errmsg "no error"
		}
	`)
	fmt.Println("Caught error:", result.String())

	// Output:
	// Direct error: error parsing regexp: missing argument to repetition operator: `*`
	// Caught error: error parsing regexp: missing argument to repetition operator: `+`
}

// TimestampType wraps time.Time and implements conversion interfaces.
// This shows how to make a custom type participate in TCL's type system.
type TimestampType struct {
	t time.Time
}

func (ts *TimestampType) Name() string         { return "timestamp" }
func (ts *TimestampType) UpdateString() string { return ts.t.Format(time.RFC3339) }
func (ts *TimestampType) Dup() feather.ObjType { return ts } // Immutable

// IntoInt returns Unix timestamp, enabling arithmetic in expr.
func (ts *TimestampType) IntoInt() (int64, bool) {
	return ts.t.Unix(), true
}

// IntoDouble returns Unix timestamp with nanosecond precision.
func (ts *TimestampType) IntoDouble() (float64, bool) {
	return float64(ts.t.UnixNano()) / 1e9, true
}

// NewTimestampObj creates a timestamp object from a time.Time.
func NewTimestampObj(t time.Time) *feather.Obj {
	return feather.NewObj(&TimestampType{t: t})
}

// GetTimestamp extracts the time.Time from an Obj.
func GetTimestamp(obj *feather.Obj) (time.Time, bool) {
	if ts, ok := obj.InternalRep().(*TimestampType); ok {
		return ts.t, true
	}
	return time.Time{}, false
}

// This example shows a custom type that implements conversion interfaces,
// allowing it to work with expr and other numeric contexts.
func Example_timestampType() {
	interp := feather.New()
	defer interp.Close()

	// Create a timestamp for a fixed point in time
	ts := NewTimestampObj(time.Date(2024, 1, 15, 10, 30, 0, 0, time.UTC))

	// Store it in a variable
	interp.SetVar("ts", ts.String())

	// The string representation is RFC3339
	result, _ := interp.Eval("set ts")
	fmt.Println("String:", result.String())

	// But we can also get the Unix timestamp via IntoInt
	n, _ := feather.AsInt(ts)
	fmt.Println("Unix:", n)

	// Output:
	// String: 2024-01-15T10:30:00Z
	// Unix: 1705314600
}
