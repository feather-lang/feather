package harness

import (
	"fmt"
	"io"
	"regexp"
)

// Config holds the configuration for running the harness.
type Config struct {
	HostPath    string
	TestPaths   []string
	NamePattern string // Go regex pattern to filter test names
	Output      io.Writer
	ErrOutput   io.Writer
	Verbose     bool
}

// testFullName returns the display name for a test case: "suite > test"
func testFullName(suite *TestSuite, tc *TestCase) string {
	return fmt.Sprintf("%s > %s", suite.Name, tc.Name)
}

// matchesFilter returns true if the test name matches the configured pattern.
// If no pattern is set, all tests match.
func matchesFilter(cfg Config, fullName string) (bool, error) {
	if cfg.NamePattern == "" {
		return true, nil
	}
	return regexp.MatchString(cfg.NamePattern, fullName)
}

// List prints all test case names from the given paths, one per line.
// Returns 0 on success, 1 on error.
func List(cfg Config) int {
	testFiles, err := CollectTestFiles(cfg.TestPaths)
	if err != nil {
		fmt.Fprintf(cfg.ErrOutput, "error: %v\n", err)
		return 1
	}

	if len(testFiles) == 0 {
		fmt.Fprintln(cfg.ErrOutput, "error: no test files found")
		return 1
	}

	for _, testFile := range testFiles {
		suite, err := ParseFile(testFile)
		if err != nil {
			fmt.Fprintf(cfg.ErrOutput, "error parsing %s: %v\n", testFile, err)
			return 1
		}

		for i := range suite.Cases {
			tc := &suite.Cases[i]
			fullName := testFullName(suite, tc)
			matches, err := matchesFilter(cfg, fullName)
			if err != nil {
				fmt.Fprintf(cfg.ErrOutput, "error: invalid pattern: %v\n", err)
				return 1
			}
			if matches {
				fmt.Fprintln(cfg.Output, fullName)
			}
		}
	}

	return 0
}

// Run executes the test harness with the given configuration.
// Returns the number of failed tests.
func Run(cfg Config) int {
	testFiles, err := CollectTestFiles(cfg.TestPaths)
	if err != nil {
		fmt.Fprintf(cfg.ErrOutput, "error: %v\n", err)
		return 1
	}

	if len(testFiles) == 0 {
		fmt.Fprintln(cfg.ErrOutput, "error: no test files found")
		return 1
	}

	runner := NewRunner(cfg.HostPath, cfg.Output)
	reporter := NewReporter(cfg.Output, cfg.Verbose)
	var allResults []TestResult
	hasErrors := false

	for _, testFile := range testFiles {
		suite, err := ParseFile(testFile)
		if err != nil {
			fmt.Fprintf(cfg.ErrOutput, "error parsing %s: %v\n", testFile, err)
			hasErrors = true
			continue
		}

		// Filter test cases by name pattern
		var filteredCases []TestCase
		for i := range suite.Cases {
			tc := &suite.Cases[i]
			fullName := testFullName(suite, tc)
			matches, err := matchesFilter(cfg, fullName)
			if err != nil {
				fmt.Fprintf(cfg.ErrOutput, "error: invalid pattern: %v\n", err)
				return 1
			}
			if matches {
				filteredCases = append(filteredCases, *tc)
			}
		}
		suite.Cases = filteredCases

		results := runner.RunSuite(suite)
		allResults = append(allResults, results...)

		for _, result := range results {
			reporter.ReportResult(testFile, result)
		}
	}

	summary := Summarize(allResults)
	reporter.ReportSummary(summary)

	if hasErrors || summary.Failed > 0 {
		return 1
	}
	return 0
}
