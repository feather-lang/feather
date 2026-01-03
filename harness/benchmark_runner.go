package harness

import (
	"bufio"
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"os/exec"
	"time"
)

// BenchmarkRunner executes benchmark suites against a host implementation.
type BenchmarkRunner struct {
	HostPath string
	Output   io.Writer
	Timeout  time.Duration // Timeout for the entire benchmark suite
}

// NewBenchmarkRunner creates a new benchmark runner for the given host executable.
func NewBenchmarkRunner(hostPath string, output io.Writer) *BenchmarkRunner {
	return &BenchmarkRunner{
		HostPath: hostPath,
		Output:   output,
		Timeout:  5 * time.Minute, // Default 5 minute timeout
	}
}

// RunSuite executes all benchmarks in a suite and returns the results.
// To minimize spawning overhead, all benchmarks are sent to a single process.
func (r *BenchmarkRunner) RunSuite(suite *BenchmarkSuite) []BenchmarkResult {
	results := make([]BenchmarkResult, 0, len(suite.Benchmarks))

	// Create a pipe for the harness communication channel (fd 3)
	harnessReader, harnessWriter, err := os.Pipe()
	if err != nil {
		// Return error results for all benchmarks
		for _, b := range suite.Benchmarks {
			results = append(results, BenchmarkResult{
				Benchmark: b,
				Success:   false,
				Error:     fmt.Sprintf("failed to create pipe: %v", err),
			})
		}
		return results
	}
	defer harnessReader.Close()

	// Create context with timeout
	ctx, cancel := context.WithTimeout(context.Background(), r.Timeout)
	defer cancel()

	// Prepare benchmark data to send to the host
	benchmarkData := prepareBenchmarkData(suite.Benchmarks)

	cmd := exec.CommandContext(ctx, r.HostPath, "--benchmark")
	cmd.Stdin = bytes.NewReader(benchmarkData)
	cmd.Env = append(os.Environ(), "FEATHER_IN_HARNESS=1")

	// Set up the extra file descriptor (will be fd 3 in the child)
	cmd.ExtraFiles = []*os.File{harnessWriter}

	var stdout, stderr bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr

	err = cmd.Start()
	if err != nil {
		harnessWriter.Close()
		for _, b := range suite.Benchmarks {
			results = append(results, BenchmarkResult{
				Benchmark: b,
				Success:   false,
				Error:     fmt.Sprintf("failed to start host: %v", err),
			})
		}
		return results
	}

	// Close the write end in the parent so we can read EOF
	harnessWriter.Close()

	// Read harness output (benchmark results)
	results = parseBenchmarkResults(harnessReader, suite.Benchmarks)

	err = cmd.Wait()
	if err != nil {
		// Check if the error was due to context timeout
		if ctx.Err() == context.DeadlineExceeded {
			for i := range results {
				if !results[i].Success {
					results[i].Error = "benchmark suite timed out"
				}
			}
		}
	}

	return results
}

// prepareBenchmarkData converts benchmarks to JSON for transmission to the host.
func prepareBenchmarkData(benchmarks []Benchmark) []byte {
	data, _ := json.Marshal(benchmarks)
	return data
}

// parseBenchmarkResults reads benchmark results from the harness channel.
func parseBenchmarkResults(r io.Reader, benchmarks []Benchmark) []BenchmarkResult {
	results := make([]BenchmarkResult, 0, len(benchmarks))
	scanner := bufio.NewScanner(r)

	for scanner.Scan() {
		line := scanner.Text()
		if line == "" {
			continue
		}

		var result BenchmarkResult
		if err := json.Unmarshal([]byte(line), &result); err != nil {
			// Failed to parse result, create error result
			if len(results) < len(benchmarks) {
				result = BenchmarkResult{
					Benchmark: benchmarks[len(results)],
					Success:   false,
					Error:     fmt.Sprintf("failed to parse result: %v", err),
				}
			}
		}
		results = append(results, result)
	}

	// If we didn't get all results, add error results for missing ones
	for len(results) < len(benchmarks) {
		results = append(results, BenchmarkResult{
			Benchmark: benchmarks[len(results)],
			Success:   false,
			Error:     "no result received from host",
		})
	}

	return results
}
