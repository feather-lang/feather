package harness

import (
	"fmt"
	"io"
)

// Reporter outputs test results.
type Reporter struct {
	Out io.Writer
}

// NewReporter creates a reporter that writes to the given output.
func NewReporter(out io.Writer) *Reporter {
	return &Reporter{Out: out}
}

// ReportResult outputs the result of a single test.
func (r *Reporter) ReportResult(testFile string, result TestResult) {
	if result.Passed {
		fmt.Fprintf(r.Out, "PASS: %s: %s\n", testFile, result.TestCase.Name)
	} else {
		fmt.Fprintf(r.Out, "FAIL: %s: %s\n", testFile, result.TestCase.Name)
		for _, failure := range result.Failures {
			fmt.Fprintf(r.Out, "  %s\n", failure)
		}
	}
}

// ReportSummary outputs the final summary.
func (r *Reporter) ReportSummary(summary Summary) {
	fmt.Fprintf(r.Out, "\n%d tests, %d passed, %d failed\n", summary.Total, summary.Passed, summary.Failed)
}
