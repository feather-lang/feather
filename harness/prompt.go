package main

import (
	"fmt"
	"os"
	"strings"
)

const promptTemplate = `# TCL Core Implementation Task

## Feature: %s

**Description:** %s

**Dependencies:** %s

**Status:** %d/%d tests passing

---

## Current Test Results

%s

## Implementation Guidelines

1. **Match TCL 9 semantics exactly** - The oracle (tclsh9) is the source of truth
2. **No memory allocation in C** - Use host callbacks for all dynamic memory
3. **Error messages must match** - TCL has specific error message formats
4. **Handle edge cases** - Empty strings, negative indices, Unicode, etc.

## Files You May Modify

%s

## Reference Files

- ` + "`core/tclc.h`" + ` - Host interface definition
- ` + "`harness/oracle/%s.json`" + ` - Expected test outputs

## Your Task

1. Analyze the failing tests and identify the root cause
1.2	 If no tests exist, use the add-tests skill to tests first.
2. Implement or fix the relevant code in the C core
3. Ensure your changes don't break other tests
4. Run ` + "`make diff FEATURE=%s`" + ` to check against oracle

Focus on making tests pass one at a time. Commit working increments.
`

// GeneratePrompt generates an agent prompt for a feature
func GeneratePrompt(featureID string) error {
	feature, err := GetFeature(featureID)
	if err != nil {
		return err
	}

	results, _ := LoadResults(featureID)
	oracle, _ := loadOracle(featureID)

	deps := "None"
	if len(feature.Depends) > 0 {
		deps = strings.Join(feature.Depends, ", ")
	}

	var passed, total int
	if results != nil {
		passed = results.Passed
		total = results.Total
	}

	testResults := formatTestResults(results, oracle)
	sourceFiles := formatSourceFiles(feature)

	prompt := fmt.Sprintf(promptTemplate,
		featureID,
		feature.Description,
		deps,
		passed, total,
		testResults,
		sourceFiles,
		featureID,
		featureID,
	)

	fmt.Print(prompt)
	return nil
}

func formatTestResults(results *DiffResult, oracle *OracleResult) string {
	if results == nil {
		return "No test results yet. Run `make diff FEATURE=<feature>` first.\n"
	}

	if len(results.Tests) == 0 {
		return "No tests defined yet. Add .tcl files to spec/tests/<feature>/\n"
	}

	if results.Passed == results.Total {
		return "All tests passing! ✓\n"
	}

	// Build map of oracle scripts
	oracleScripts := make(map[string]string)
	if oracle != nil {
		for _, t := range oracle.Tests {
			oracleScripts[t.Name] = t.Script
		}
	}

	var sb strings.Builder
	sb.WriteString("### Failing Tests\n\n")

	for _, test := range results.Tests {
		if test.Passed {
			continue
		}

		sb.WriteString(fmt.Sprintf("#### Test: %s\n\n", test.Name))

		if script, ok := oracleScripts[test.Name]; ok && script != "" {
			sb.WriteString("**Script:**\n```tcl\n")
			sb.WriteString(strings.TrimSpace(script))
			sb.WriteString("\n```\n\n")
		}

		sb.WriteString("**Expected output:**\n```\n")
		if test.Expected.Stdout == "" {
			sb.WriteString("(empty)")
		} else {
			sb.WriteString(test.Expected.Stdout)
		}
		sb.WriteString("\n```\n\n")

		if test.Expected.Stderr != "" {
			sb.WriteString("**Expected stderr:**\n```\n")
			sb.WriteString(test.Expected.Stderr)
			sb.WriteString("\n```\n\n")
		}

		sb.WriteString(fmt.Sprintf("**Expected return code:** %d\n\n", test.Expected.ReturnCode))

		sb.WriteString("**Actual output:**\n```\n")
		if test.Actual.Stdout == "" {
			sb.WriteString("(empty)")
		} else {
			sb.WriteString(test.Actual.Stdout)
		}
		sb.WriteString("\n```\n\n")

		if test.Actual.Stderr != "" {
			sb.WriteString("**Actual stderr:**\n```\n")
			sb.WriteString(test.Actual.Stderr)
			sb.WriteString("\n```\n\n")
		}

		sb.WriteString(fmt.Sprintf("**Actual return code:** %d\n\n", test.Actual.ReturnCode))

		if test.Diff != "" {
			sb.WriteString("**Diff:**\n```diff\n")
			sb.WriteString(test.Diff)
			sb.WriteString("```\n\n")
		}

		if test.Error != "" {
			sb.WriteString(fmt.Sprintf("**Error:** %s\n\n", test.Error))
		}

		sb.WriteString("---\n\n")
	}

	return sb.String()
}

func formatSourceFiles(feature *Feature) string {
	if len(feature.SourceFiles) == 0 {
		return "- `core/*.c` - Implementation files\n"
	}

	var sb strings.Builder
	for _, f := range feature.SourceFiles {
		if f.Description != "" {
			sb.WriteString(fmt.Sprintf("- `%s` - %s\n", f.Path, f.Description))
		} else {
			sb.WriteString(fmt.Sprintf("- `%s`\n", f.Path))
		}
	}
	return sb.String()
}

// SavePromptToFile generates prompt and saves to file
func SavePromptToFile(featureID, filename string) error {
	// Capture stdout
	old := os.Stdout
	r, w, _ := os.Pipe()
	os.Stdout = w

	err := GeneratePrompt(featureID)

	w.Close()
	os.Stdout = old

	if err != nil {
		return err
	}

	buf := make([]byte, 64*1024)
	n, _ := r.Read(buf)

	return os.WriteFile(filename, buf[:n], 0644)
}
