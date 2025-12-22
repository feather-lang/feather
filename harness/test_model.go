package harness

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
}

// TestSuite represents a collection of test cases parsed from an HTML file.
type TestSuite struct {
	Path  string
	Cases []TestCase
}
