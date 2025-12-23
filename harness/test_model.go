package harness

import "time"

// DefaultTimeout is the default timeout for test cases when not specified.
const DefaultTimeout = 1 * time.Second

// TestCase captures the relevant information about
// a single test case in the harness.
type TestCase struct {
	Name      string
	Script    string
	Return    string // The return code: TCL_OK, TCL_ERROR, etc.
	Result    string // The interpreter's result object string representation
	Error     string
	Stdout    string
	StdoutSet bool // True if <stdout> was present in test file
	Stderr    string
	ExitCode  int
	Timeout   time.Duration // Timeout for this test case (0 means use suite/default)
}

// TestSuite represents a collection of test cases parsed from an HTML file.
type TestSuite struct {
	Name    string
	Path    string
	Cases   []TestCase
	Timeout time.Duration // Default timeout for test cases in this suite (0 means use global default)
}
