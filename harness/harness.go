package harness

import (
	"fmt"
	"io"
	"os"
	"regexp"
	"strconv"
	"strings"
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

// Update runs tests against the host and updates test file expectations.
// Returns 0 on success, 1 on error.
func Update(cfg Config) int {
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
	totalUpdated := 0

	for _, testFile := range testFiles {
		suite, err := ParseFile(testFile)
		if err != nil {
			fmt.Fprintf(cfg.ErrOutput, "error parsing %s: %v\n", testFile, err)
			continue
		}

		// Filter test cases by name pattern
		var indicesToUpdate []int
		for i := range suite.Cases {
			tc := &suite.Cases[i]
			fullName := testFullName(suite, tc)
			matches, err := matchesFilter(cfg, fullName)
			if err != nil {
				fmt.Fprintf(cfg.ErrOutput, "error: invalid pattern: %v\n", err)
				return 1
			}
			if matches {
				indicesToUpdate = append(indicesToUpdate, i)
			}
		}

		if len(indicesToUpdate) == 0 {
			continue
		}

		// Read the original file content
		content, err := os.ReadFile(testFile)
		if err != nil {
			fmt.Fprintf(cfg.ErrOutput, "error reading %s: %v\n", testFile, err)
			continue
		}

		fileContent := string(content)
		fileUpdated := false

		// Run each matching test and update expectations
		for _, idx := range indicesToUpdate {
			tc := suite.Cases[idx]
			result := runner.RunTest(tc)

			if !result.Passed {
				// Update the file content with actual results
				newContent := updateTestCase(fileContent, tc.Name, result.Actual)
				if newContent != fileContent {
					fileContent = newContent
					fileUpdated = true
					totalUpdated++
					fmt.Fprintf(cfg.Output, "updated: %s > %s\n", suite.Name, tc.Name)
				}
			}
		}

		// Write back the file if updated
		if fileUpdated {
			err = os.WriteFile(testFile, []byte(fileContent), 0644)
			if err != nil {
				fmt.Fprintf(cfg.ErrOutput, "error writing %s: %v\n", testFile, err)
				continue
			}
		}
	}

	fmt.Fprintf(cfg.Output, "\n%d test(s) updated\n", totalUpdated)
	return 0
}

// updateTestCase updates a single test case in the file content.
// It finds the test case by name and replaces its expected values.
func updateTestCase(content, testName string, actual ActualResult) string {
	// Find the test-case element with this name
	// We'll do a simple string-based replacement to preserve formatting

	// Find the start of this test case
	nameAttr := fmt.Sprintf(`name="%s"`, testName)
	idx := strings.Index(content, nameAttr)
	if idx == -1 {
		nameAttr = fmt.Sprintf(`name='%s'`, testName)
		idx = strings.Index(content, nameAttr)
	}
	if idx == -1 {
		return content
	}

	// Find the end of this test case
	testCaseEnd := strings.Index(content[idx:], "</test-case>")
	if testCaseEnd == -1 {
		return content
	}
	testCaseEnd += idx + len("</test-case>")

	// Find the start of the test-case tag
	testCaseStart := strings.LastIndex(content[:idx], "<test-case")
	if testCaseStart == -1 {
		return content
	}

	testCaseContent := content[testCaseStart:testCaseEnd]

	// Update individual elements
	updated := testCaseContent

	// Update stdout
	updated = updateElement(updated, "stdout", actual.Stdout)

	// Update stderr
	updated = updateElement(updated, "stderr", actual.Stderr)

	// Update exit-code
	updated = updateElement(updated, "exit-code", strconv.Itoa(actual.ExitCode))

	// Update return
	if actual.Return != "" {
		updated = updateElement(updated, "return", actual.Return)
	}

	// Update error
	updated = updateElement(updated, "error", actual.Error)

	// Replace the test case in the content
	return content[:testCaseStart] + updated + content[testCaseEnd:]
}

// updateElement updates a specific element within a test case.
func updateElement(content, elemName, newValue string) string {
	startTag := "<" + elemName + ">"
	endTag := "</" + elemName + ">"

	startIdx := strings.Index(content, startTag)
	if startIdx == -1 {
		return content
	}
	startIdx += len(startTag)

	endIdx := strings.Index(content[startIdx:], endTag)
	if endIdx == -1 {
		return content
	}
	endIdx += startIdx

	return content[:startIdx] + newValue + content[endIdx:]
}
