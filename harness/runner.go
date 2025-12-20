package harness

import (
	"bytes"
	"fmt"
	"io"
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

	cmd := exec.Command(r.HostPath)
	cmd.Stdin = strings.NewReader(tc.Script)

	var stdout, stderr bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr

	err := cmd.Run()

	result.Actual.Stdout = strings.TrimSpace(stdout.String())
	result.Actual.Stderr = strings.TrimSpace(stderr.String())

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

	return result
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
