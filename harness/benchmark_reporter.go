package harness

import (
	"fmt"
	"io"
	"time"
)

// BenchmarkReporter handles reporting of benchmark results.
type BenchmarkReporter struct {
	output io.Writer
}

// NewBenchmarkReporter creates a new benchmark reporter.
func NewBenchmarkReporter(output io.Writer) *BenchmarkReporter {
	return &BenchmarkReporter{output: output}
}

// ReportSuite reports the results of an entire benchmark suite.
func (r *BenchmarkReporter) ReportSuite(suite *BenchmarkSuite, results []BenchmarkResult) {
	fmt.Fprintf(r.output, "\n=== Benchmark Suite: %s ===\n\n", suite.Name)

	for _, result := range results {
		r.reportBenchmark(result)
	}

	// Summary
	totalSuccess := 0
	totalFailed := 0
	for _, result := range results {
		if result.Success {
			totalSuccess++
		} else {
			totalFailed++
		}
	}

	fmt.Fprintf(r.output, "\n--- Summary ---\n")
	fmt.Fprintf(r.output, "Total: %d  Success: %d  Failed: %d\n\n",
		len(results), totalSuccess, totalFailed)
}

// reportBenchmark reports a single benchmark result.
func (r *BenchmarkReporter) reportBenchmark(result BenchmarkResult) {
	if !result.Success {
		fmt.Fprintf(r.output, "FAIL: %s\n", result.Benchmark.Name)
		fmt.Fprintf(r.output, "  Error: %s\n\n", result.Error)
		return
	}

	fmt.Fprintf(r.output, "PASS: %s\n", result.Benchmark.Name)
	fmt.Fprintf(r.output, "  Iterations: %d\n", result.Iterations)
	fmt.Fprintf(r.output, "  Total time: %s\n", formatDuration(result.TotalTime))
	fmt.Fprintf(r.output, "  Avg time:   %s/op\n", formatDuration(result.AvgTime))
	fmt.Fprintf(r.output, "  Min time:   %s\n", formatDuration(result.MinTime))
	fmt.Fprintf(r.output, "  Max time:   %s\n", formatDuration(result.MaxTime))
	fmt.Fprintf(r.output, "  Ops/sec:    %.2f\n\n", result.OpsPerSecond)
}

// formatDuration formats a duration in a human-readable way.
func formatDuration(d time.Duration) string {
	// Choose appropriate unit
	if d < time.Microsecond {
		return fmt.Sprintf("%dns", d.Nanoseconds())
	}
	if d < time.Millisecond {
		return fmt.Sprintf("%.2fµs", float64(d.Nanoseconds())/1000.0)
	}
	if d < time.Second {
		return fmt.Sprintf("%.2fms", float64(d.Nanoseconds())/1000000.0)
	}
	return fmt.Sprintf("%.2fs", d.Seconds())
}
