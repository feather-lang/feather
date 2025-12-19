package main

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"
)

// RunDiff compares our implementation against oracle
func RunDiff(featureID string) error {
	// Load oracle
	oracle, err := loadOracle(featureID)
	if err != nil {
		result := DiffResult{
			Feature: featureID,
			Error:   err.Error(),
		}
		saveResults(featureID, result)
		return err
	}

	if len(oracle.Tests) == 0 {
		fmt.Printf("No tests in oracle for feature %q\n", featureID)
		result := DiffResult{
			Feature: featureID,
			Error:   "no_tests",
		}
		saveResults(featureID, result)
		return nil
	}

	interp := os.Getenv("TCLC_INTERP")
	if interp == "" {
		fmt.Println("  Note: TCLC_INTERP not set, using placeholder results")
	}

	fmt.Printf("Running differential tests for %s...\n", featureID)

	result := DiffResult{
		Feature: featureID,
		Tests:   make([]TestDiff, 0, len(oracle.Tests)),
	}

	for _, oracleTest := range oracle.Tests {
		diff := compareTest(interp, oracleTest)
		result.Tests = append(result.Tests, diff)

		if diff.Passed {
			result.Passed++
			fmt.Printf("  ✓ %s\n", diff.Name)
		} else {
			result.Failed++
			fmt.Printf("  ✗ %s\n", diff.Name)
			if diff.Error != "" {
				fmt.Printf("    Error: %s\n", diff.Error)
			}
		}
	}

	result.Total = len(result.Tests)

	// Save results
	if err := saveResults(featureID, result); err != nil {
		return err
	}

	// Summary
	fmt.Println()
	fmt.Printf("Results: %d/%d tests passing\n", result.Passed, result.Total)

	return nil
}

func loadOracle(featureID string) (*OracleResult, error) {
	oracleFile := filepath.Join("harness", "oracle", featureID+".json")
	data, err := os.ReadFile(oracleFile)
	if err != nil {
		if os.IsNotExist(err) {
			return nil, fmt.Errorf("no oracle found for feature %q\nRun: make oracle FEATURE=%s", featureID, featureID)
		}
		return nil, fmt.Errorf("reading oracle: %w", err)
	}

	var oracle OracleResult
	if err := json.Unmarshal(data, &oracle); err != nil {
		return nil, fmt.Errorf("parsing oracle: %w", err)
	}

	return &oracle, nil
}

func compareTest(interp string, oracle OracleTest) TestDiff {
	actual := runOurInterp(interp, oracle.Script)

	stdoutMatch := actual.Stdout == oracle.Stdout
	codeMatch := actual.ReturnCode == oracle.ReturnCode

	passed := stdoutMatch && codeMatch

	diff := TestDiff{
		Name:   oracle.Name,
		Passed: passed,
		Expected: TestOutput{
			Stdout:     oracle.Stdout,
			Stderr:     oracle.Stderr,
			ReturnCode: oracle.ReturnCode,
		},
		Actual: actual,
	}

	if !passed {
		diff.Diff = computeDiff(oracle.Stdout, actual.Stdout)
	}

	return diff
}

func runOurInterp(interp, script string) TestOutput {
	if interp == "" {
		return TestOutput{
			Stdout:     "",
			Stderr:     "Interpreter not implemented yet",
			ReturnCode: -1,
		}
	}

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	cmd := exec.CommandContext(ctx, interp)
	cmd.Stdin = strings.NewReader(script)

	stdout, err := cmd.Output()
	result := TestOutput{
		Stdout: string(stdout),
	}

	if err != nil {
		if exitErr, ok := err.(*exec.ExitError); ok {
			result.Stderr = string(exitErr.Stderr)
			result.ReturnCode = exitErr.ExitCode()
		} else if ctx.Err() == context.DeadlineExceeded {
			result.Stderr = "timeout"
			result.ReturnCode = -1
		} else {
			result.Stderr = err.Error()
			result.ReturnCode = -1
		}
	}

	return result
}

func computeDiff(expected, actual string) string {
	expectedLines := strings.Split(expected, "\n")
	actualLines := strings.Split(actual, "\n")

	var diff strings.Builder
	diff.WriteString("--- expected\n")
	diff.WriteString("+++ actual\n")

	maxLines := len(expectedLines)
	if len(actualLines) > maxLines {
		maxLines = len(actualLines)
	}

	for i := 0; i < maxLines; i++ {
		var expLine, actLine string
		if i < len(expectedLines) {
			expLine = expectedLines[i]
		}
		if i < len(actualLines) {
			actLine = actualLines[i]
		}

		if expLine != actLine {
			if expLine != "" {
				diff.WriteString("-" + expLine + "\n")
			}
			if actLine != "" {
				diff.WriteString("+" + actLine + "\n")
			}
		}
	}

	return diff.String()
}

func saveResults(featureID string, result DiffResult) error {
	resultsDir := filepath.Join("harness", "results")
	if err := os.MkdirAll(resultsDir, 0755); err != nil {
		return fmt.Errorf("creating results directory: %w", err)
	}

	resultsFile := filepath.Join(resultsDir, featureID+".json")
	data, err := json.MarshalIndent(result, "", "  ")
	if err != nil {
		return fmt.Errorf("encoding results: %w", err)
	}

	if err := os.WriteFile(resultsFile, data, 0644); err != nil {
		return fmt.Errorf("writing results: %w", err)
	}

	fmt.Printf("Saved to %s\n", resultsFile)
	return nil
}

// LoadResults loads test results for a feature
func LoadResults(featureID string) (*DiffResult, error) {
	resultsFile := filepath.Join("harness", "results", featureID+".json")
	data, err := os.ReadFile(resultsFile)
	if err != nil {
		if os.IsNotExist(err) {
			return nil, nil
		}
		return nil, err
	}

	var result DiffResult
	if err := json.Unmarshal(data, &result); err != nil {
		return nil, err
	}

	return &result, nil
}
