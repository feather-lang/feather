package harness

// TestCase captures the relevant information about
// a single test case in the harness.
type TestCase struct {
	Name     string
	Script   string
	Result   string
	Error    string
	Stdout   string
	Stderr   string
	ExitCode int
}

// TestSuite represents a collection of test cases parsed from an HTML file.
type TestSuite struct {
	Path  string
	Cases []TestCase
}
