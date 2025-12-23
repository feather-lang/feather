package harness

import (
	"fmt"
	"io"
	"strings"
)

// Reporter outputs test results.
type Reporter struct {
	Out     io.Writer
	Verbose bool
}

// NewReporter creates a reporter that writes to the given output.
func NewReporter(out io.Writer, verbose bool) *Reporter {
	return &Reporter{Out: out, Verbose: verbose}
}

// ReportResult outputs the result of a single test.
func (r *Reporter) ReportResult(testFile string, result TestResult) {
	if result.Passed {
		if r.Verbose {
			fmt.Fprintf(r.Out, "PASS: %s: %s\n", testFile, result.TestCase.Name)
		}
	} else {
		fmt.Fprintf(r.Out, "FAIL: %s: %s\n", testFile, result.TestCase.Name)
		for _, failure := range result.Failures {
			fmt.Fprintf(r.Out, "  %s\n", failure)
		}
		fmt.Fprintf(r.Out, "  script:\n")
		fmt.Fprintf(r.Out, "    %s\n", indentScript(result.TestCase.Script))
	}
}

// ReportSummary outputs the final summary.
func (r *Reporter) ReportSummary(summary Summary) {
	fmt.Fprintf(r.Out, "\n%d tests, %d passed, %d failed\n", summary.Total, summary.Passed, summary.Failed)
}

// indentScript adds indentation to each line of a multi-line script.
func indentScript(script string) string {
	lines := strings.Split(strings.TrimSpace(script), "\n")
	return strings.Join(lines, "\n    ")
}
