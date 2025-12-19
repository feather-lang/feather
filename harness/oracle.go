package main

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"sort"
	"time"
)

const defaultTclsh = "tclsh"

// GenerateOracle runs tests against real tclsh and saves expected outputs
func GenerateOracle(featureID string) error {
	tclsh := os.Getenv("TCLSH")
	if tclsh == "" {
		tclsh = defaultTclsh
	}

	// Find test files
	testDir := filepath.Join("spec", "tests", featureID)
	entries, err := os.ReadDir(testDir)
	if err != nil {
		if os.IsNotExist(err) {
			fmt.Printf("No tests found for feature %q\n", featureID)
			fmt.Printf("Add .tcl files to %s/\n", testDir)
			if err := os.MkdirAll(testDir, 0755); err != nil {
				return err
			}
			fmt.Println("Created test directory.")
			return nil
		}
		return fmt.Errorf("reading test directory: %w", err)
	}

	var testFiles []string
	for _, e := range entries {
		if !e.IsDir() && filepath.Ext(e.Name()) == ".tcl" {
			testFiles = append(testFiles, filepath.Join(testDir, e.Name()))
		}
	}

	if len(testFiles) == 0 {
		fmt.Printf("No .tcl files found in %s/\n", testDir)
		return nil
	}

	sort.Strings(testFiles)

	fmt.Printf("Generating oracle for %s (%d tests)...\n", featureID, len(testFiles))

	result := OracleResult{
		Feature: featureID,
		Tclsh:   tclsh,
		Tests:   make([]OracleTest, 0, len(testFiles)),
	}

	for _, testFile := range testFiles {
		fmt.Printf("  Running %s...\n", filepath.Base(testFile))

		test, err := runOracleTest(tclsh, testFile)
		if err != nil {
			fmt.Printf("    Error: %v\n", err)
		} else if test.Error != "" {
			fmt.Printf("    Error: %s\n", test.Error)
		} else if test.ReturnCode != 0 {
			fmt.Printf("    Exit code: %d\n", test.ReturnCode)
		}

		result.Tests = append(result.Tests, test)
	}

	// Save oracle
	oracleDir := filepath.Join("harness", "oracle")
	if err := os.MkdirAll(oracleDir, 0755); err != nil {
		return fmt.Errorf("creating oracle directory: %w", err)
	}

	oracleFile := filepath.Join(oracleDir, featureID+".json")
	data, err := json.MarshalIndent(result, "", "  ")
	if err != nil {
		return fmt.Errorf("encoding oracle: %w", err)
	}

	if err := os.WriteFile(oracleFile, data, 0644); err != nil {
		return fmt.Errorf("writing oracle: %w", err)
	}

	fmt.Printf("Oracle saved to %s\n", oracleFile)
	return nil
}

func runOracleTest(tclsh, testFile string) (OracleTest, error) {
	script, err := os.ReadFile(testFile)
	if err != nil {
		return OracleTest{
			Name:  filepath.Base(testFile[:len(testFile)-4]),
			File:  testFile,
			Error: fmt.Sprintf("reading file: %v", err),
		}, err
	}

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	cmd := exec.CommandContext(ctx, tclsh, testFile)
	stdout, err := cmd.Output()

	test := OracleTest{
		Name:   filepath.Base(testFile[:len(testFile)-4]),
		File:   testFile,
		Script: string(script),
		Stdout: string(stdout),
	}

	if err != nil {
		if exitErr, ok := err.(*exec.ExitError); ok {
			test.Stderr = string(exitErr.Stderr)
			test.ReturnCode = exitErr.ExitCode()
		} else if ctx.Err() == context.DeadlineExceeded {
			test.Error = "timeout"
			test.ReturnCode = -1
		} else {
			test.Error = err.Error()
			test.ReturnCode = -1
		}
	}

	return test, nil
}

// GenerateAllOracles generates oracles for all features
func GenerateAllOracles() error {
	features, err := LoadFeatures()
	if err != nil {
		return err
	}

	for _, f := range features {
		if err := GenerateOracle(f.ID); err != nil {
			fmt.Printf("Error generating oracle for %s: %v\n", f.ID, err)
		}
	}

	return nil
}
