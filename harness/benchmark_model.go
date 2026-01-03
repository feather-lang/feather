package harness

import "time"

// Benchmark captures information about a single benchmark.
type Benchmark struct {
	Name       string
	Setup      string // Script to run once before iterations (optional)
	Script     string // The script to benchmark
	Warmup     int    // Number of warmup iterations
	Iterations int    // Number of measured iterations
}

// BenchmarkSuite represents a collection of benchmarks.
type BenchmarkSuite struct {
	Name       string
	Path       string
	Benchmarks []Benchmark
}

// BenchmarkResult holds the outcome of running a single benchmark.
type BenchmarkResult struct {
	Benchmark    Benchmark
	Success      bool
	TotalTime    time.Duration // Total time for all iterations
	AvgTime      time.Duration // Average time per iteration
	MinTime      time.Duration // Minimum time for a single iteration
	MaxTime      time.Duration // Maximum time for a single iteration
	Iterations   int           // Actual number of iterations completed
	OpsPerSecond float64       // Operations per second
	Error        string        // Error message if benchmark failed
}
