package main

// Feature represents a TCL feature to implement
type Feature struct {
	ID          string       `yaml:"id" json:"id"`
	Description string       `yaml:"description" json:"description"`
	Depends     []string     `yaml:"depends" json:"depends"`
	Phase       int          `yaml:"phase" json:"phase"`
	Status      string       `yaml:"status" json:"status"`
	Tests       []string     `yaml:"tests" json:"tests"`
	SourceFiles []SourceFile `yaml:"source_files" json:"source_files"`
}

// SourceFile represents a file that can be modified for a feature
type SourceFile struct {
	Path        string `yaml:"path" json:"path"`
	Description string `yaml:"description" json:"description"`
}

// FeaturesFile represents the spec/features.yaml structure
type FeaturesFile struct {
	Features []Feature `yaml:"features" json:"features"`
}

// OracleResult represents expected output from tclsh
type OracleResult struct {
	Feature string       `json:"feature"`
	Tclsh   string       `json:"tclsh"`
	Tests   []OracleTest `json:"tests"`
}

// OracleTest represents a single test's expected output
type OracleTest struct {
	Name       string `json:"name"`
	File       string `json:"file"`
	Script     string `json:"script"`
	Stdout     string `json:"stdout"`
	Stderr     string `json:"stderr"`
	ReturnCode int    `json:"returncode"`
	Error      string `json:"error,omitempty"`
}

// DiffResult represents differential test results
type DiffResult struct {
	Feature string     `json:"feature"`
	Passed  int        `json:"passed"`
	Failed  int        `json:"failed"`
	Total   int        `json:"total"`
	Tests   []TestDiff `json:"tests"`
	Error   string     `json:"error,omitempty"`
}

// TestDiff represents comparison of one test
type TestDiff struct {
	Name     string     `json:"name"`
	Passed   bool       `json:"passed"`
	Expected TestOutput `json:"expected"`
	Actual   TestOutput `json:"actual"`
	Diff     string     `json:"diff,omitempty"`
	Error    string     `json:"error,omitempty"`
}

// TestOutput represents stdout/stderr/returncode
type TestOutput struct {
	Stdout     string `json:"stdout"`
	Stderr     string `json:"stderr"`
	ReturnCode int    `json:"returncode"`
}
