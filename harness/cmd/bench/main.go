// bench runs benchmarks against a feather host implementation.
package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"

	"github.com/feather-lang/feather/harness"
)

func main() {
	var hostPath string
	flag.StringVar(&hostPath, "host", "", "Path to the host executable")
	flag.Parse()

	if hostPath == "" {
		fmt.Fprintf(os.Stderr, "Usage: bench -host <host-executable> <benchmark-files...>\n")
		os.Exit(1)
	}

	if flag.NArg() == 0 {
		fmt.Fprintf(os.Stderr, "Error: no benchmark files specified\n")
		os.Exit(1)
	}

	runner := harness.NewBenchmarkRunner(hostPath, os.Stdout)
	reporter := harness.NewBenchmarkReporter(os.Stdout)

	allSuccess := true
	for _, path := range flag.Args() {
		suite, err := harness.ParseBenchmarkFile(path)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Error parsing %s: %v\n", path, err)
			allSuccess = false
			continue
		}

		// Resolve relative paths from the benchmark file directory
		origPath := suite.Path
		suite.Path, _ = filepath.Abs(origPath)

		results := runner.RunSuite(suite)
		reporter.ReportSuite(suite, results)

		// Check if any benchmark failed
		for _, result := range results {
			if !result.Success {
				allSuccess = false
			}
		}
	}

	if !allSuccess {
		os.Exit(1)
	}
}
