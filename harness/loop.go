package main

import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

// RunLoop runs the feedback loop for a feature
func RunLoop(featureID string) error {
	fmt.Printf("Starting feedback loop for %s\n", featureID)
	fmt.Println(strings.Repeat("=", 50))

	// Check for tests
	testDir := filepath.Join("spec", "tests", featureID)
	if !testsExist(testDir) {
		fmt.Printf("\nNo tests found for %s\n", featureID)
		fmt.Printf("Add .tcl test files to %s/\n", testDir)
		fmt.Println("\nCreating test directory...")
		if err := os.MkdirAll(testDir, 0755); err != nil {
			return err
		}
		fmt.Printf("\nExample test file (%s/basic.tcl):\n", testDir)
		fmt.Println("```tcl")
		fmt.Println("# Test basic functionality")
		fmt.Println(`puts "Hello, World!"`)
		fmt.Println("```")
		return nil
	}

	// Generate oracle if needed
	oracleFile := filepath.Join("harness", "oracle", featureID+".json")
	if _, err := os.Stat(oracleFile); os.IsNotExist(err) {
		fmt.Println("\nGenerating oracle...")
		if err := GenerateOracle(featureID); err != nil {
			return fmt.Errorf("generating oracle: %w", err)
		}
	}

	iteration := 0
	reader := bufio.NewReader(os.Stdin)

	for {
		iteration++
		fmt.Printf("\n--- Iteration %d ---\n", iteration)

		// Run differential tests
		if err := RunDiff(featureID); err != nil {
			fmt.Printf("Error running diff: %v\n", err)
		}

		// Check results
		results, err := LoadResults(featureID)
		if err != nil {
			return fmt.Errorf("loading results: %w", err)
		}

		if results == nil || results.Total == 0 {
			fmt.Println("\nNo tests to run. Add tests and regenerate oracle.")
			return nil
		}

		if results.Passed == results.Total {
			fmt.Println()
			fmt.Println(strings.Repeat("=", 50))
			fmt.Printf("✓ %s complete - all %d tests passing!\n", featureID, results.Total)
			fmt.Println(strings.Repeat("=", 50))
			return nil
		}

		fmt.Printf("\n%d/%d tests passing\n", results.Passed, results.Total)

		// Generate prompt
		fmt.Println("\nGenerating agent prompt...")
		if err := SavePromptToFile(featureID, "prompt.md"); err != nil {
			fmt.Printf("Error generating prompt: %v\n", err)
		} else {
			fmt.Println("Prompt written to prompt.md")
		}

		fmt.Println()
		fmt.Println(strings.Repeat("-", 50))
		fmt.Println("Next steps:")
		fmt.Println("  1. Review prompt.md for failing test details")
		fmt.Println("  2. Implement/fix the code")
		fmt.Println("  3. Press Enter to re-run tests")
		fmt.Println("  (Ctrl+C to abort)")
		fmt.Println(strings.Repeat("-", 50))

		fmt.Print("\nPress Enter to continue...")
		_, err = reader.ReadString('\n')
		if err != nil {
			fmt.Println("\n\nAborted.")
			return nil
		}
	}
}

func testsExist(testDir string) bool {
	entries, err := os.ReadDir(testDir)
	if err != nil {
		return false
	}

	for _, e := range entries {
		if !e.IsDir() && filepath.Ext(e.Name()) == ".tcl" {
			return true
		}
	}

	return false
}
