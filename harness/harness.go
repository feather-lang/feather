package harness

import (
	"fmt"
	"io"
)

// Config holds the configuration for running the harness.
type Config struct {
	HostPath  string
	TestPaths []string
	Output    io.Writer
	ErrOutput io.Writer
	Verbose   bool
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
