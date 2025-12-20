package harness

import (
	"bufio"
	"bytes"
	"fmt"
	"io"
	"os"
	"os/exec"
	"strings"
)

// TestResult holds the outcome of running a single test case.
type TestResult struct {
	TestCase TestCase
	Passed   bool
	Actual   ActualResult
	Failures []string
}

// ActualResult captures what actually happened when the test ran.
type ActualResult struct {
	Stdout   string
	Stderr   string
	ExitCode int
	Result   string // TCL_OK, TCL_ERROR, etc.
	Error    string // Error message from interpreter
}

// Runner executes test suites against a host implementation.
type Runner struct {
	HostPath string
	Output   io.Writer
}

// NewRunner creates a new test runner for the given host executable.
func NewRunner(hostPath string, output io.Writer) *Runner {
	return &Runner{
		HostPath: hostPath,
		Output:   output,
	}
}

// RunSuite executes all test cases in a suite and returns the results.
func (r *Runner) RunSuite(suite *TestSuite) []TestResult {
	results := make([]TestResult, 0, len(suite.Cases))
	for _, tc := range suite.Cases {
		result := r.RunTest(tc)
		results = append(results, result)
	}
	return results
}

// RunTest executes a single test case and returns the result.
func (r *Runner) RunTest(tc TestCase) TestResult {
	result := TestResult{
		TestCase: tc,
		Passed:   true,
	}

	// Create a pipe for the harness communication channel (fd 3)
	harnessReader, harnessWriter, err := os.Pipe()
	if err != nil {
		result.Passed = false
		result.Failures = append(result.Failures, fmt.Sprintf("failed to create pipe: %v", err))
		return result
	}
	defer harnessReader.Close()

	cmd := exec.Command(r.HostPath)
	cmd.Stdin = strings.NewReader(tc.Script)
	cmd.Env = append(os.Environ(), "TCLC_IN_HARNESS=1")

	// Set up the extra file descriptor (will be fd 3 in the child)
	cmd.ExtraFiles = []*os.File{harnessWriter}

	var stdout, stderr bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr

	err = cmd.Start()
	if err != nil {
		harnessWriter.Close()
		result.Passed = false
		result.Failures = append(result.Failures, fmt.Sprintf("failed to start host: %v", err))
		return result
	}

	// Close the write end in the parent so we can read EOF
	harnessWriter.Close()

	// Read harness output
	harnessOutput := parseHarnessOutput(harnessReader)

	err = cmd.Wait()

	result.Actual.Stdout = normalizeLines(stdout.String())
	result.Actual.Stderr = normalizeLines(stderr.String())
	result.Actual.Result = harnessOutput.Result
	result.Actual.Error = harnessOutput.Error

	if err != nil {
		if exitErr, ok := err.(*exec.ExitError); ok {
			result.Actual.ExitCode = exitErr.ExitCode()
		} else {
			result.Passed = false
			result.Failures = append(result.Failures, fmt.Sprintf("failed to run host: %v", err))
			return result
		}
	}

	// Compare results
	if tc.Stdout != result.Actual.Stdout {
		result.Passed = false
		result.Failures = append(result.Failures,
			fmt.Sprintf("stdout mismatch:\n  expected: %q\n  actual:   %q", tc.Stdout, result.Actual.Stdout))
	}

	if tc.Stderr != result.Actual.Stderr {
		result.Passed = false
		result.Failures = append(result.Failures,
			fmt.Sprintf("stderr mismatch:\n  expected: %q\n  actual:   %q", tc.Stderr, result.Actual.Stderr))
	}

	if tc.ExitCode != result.Actual.ExitCode {
		result.Passed = false
		result.Failures = append(result.Failures,
			fmt.Sprintf("exit code mismatch:\n  expected: %d\n  actual:   %d", tc.ExitCode, result.Actual.ExitCode))
	}

	// Compare result code if specified in test case
	if tc.Result != "" && tc.Result != result.Actual.Result {
		result.Passed = false
		result.Failures = append(result.Failures,
			fmt.Sprintf("result mismatch:\n  expected: %q\n  actual:   %q", tc.Result, result.Actual.Result))
	}

	// Compare error message if specified in test case
	if tc.Error != "" && tc.Error != result.Actual.Error {
		result.Passed = false
		result.Failures = append(result.Failures,
			fmt.Sprintf("error mismatch:\n  expected: %q\n  actual:   %q", tc.Error, result.Actual.Error))
	}

	return result
}

// harnessOutput holds parsed output from the harness channel
type harnessOutput struct {
	Result string
	Error  string
}

// parseHarnessOutput reads and parses the harness channel output
func parseHarnessOutput(r io.Reader) harnessOutput {
	var out harnessOutput
	scanner := bufio.NewScanner(r)
	for scanner.Scan() {
		line := scanner.Text()
		if strings.HasPrefix(line, "result: ") {
			out.Result = strings.TrimPrefix(line, "result: ")
		} else if strings.HasPrefix(line, "error: ") {
			out.Error = strings.TrimPrefix(line, "error: ")
		}
	}
	return out
}

// Summary holds aggregate statistics about a test run.
type Summary struct {
	Total  int
	Passed int
	Failed int
}

// Summarize calculates summary statistics from test results.
func Summarize(results []TestResult) Summary {
	s := Summary{Total: len(results)}
	for _, r := range results {
		if r.Passed {
			s.Passed++
		} else {
			s.Failed++
		}
	}
	return s
}
