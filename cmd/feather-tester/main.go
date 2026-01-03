// feather-tester is the interpreter used by the test harness.
// It includes test-specific commands (say-hello, echo, count) and types (Counter).
package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"time"

	"github.com/feather-lang/feather"
)

// Counter is a simple foreign object type for testing.
type Counter struct {
	value int
}

// Benchmark represents a benchmark to run (mirrors harness.Benchmark).
type Benchmark struct {
	Name       string
	Setup      string
	Script     string
	Warmup     int
	Iterations int
}

// BenchmarkResult holds the outcome of running a benchmark (mirrors harness.BenchmarkResult).
type BenchmarkResult struct {
	Benchmark    Benchmark
	Success      bool
	TotalTime    time.Duration
	AvgTime      time.Duration
	MinTime      time.Duration
	MaxTime      time.Duration
	Iterations   int
	OpsPerSecond float64
	Error        string
}

func main() {
	// Check for benchmark mode
	if len(os.Args) > 1 && os.Args[1] == "--benchmark" {
		runBenchmarkMode()
		return
	}

	i := feather.New()
	defer i.Close()

	// Register test-specific commands
	registerTestCommands(i)

	// Check if stdin is a TTY
	stat, _ := os.Stdin.Stat()
	if (stat.Mode() & os.ModeCharDevice) != 0 {
		runREPL(i)
		return
	}

	runScript(i)
}

func registerTestCommands(i *feather.Interp) {
	// Set milestone variables
	i.SetVar("milestone", "m1")
	i.SetVar("current-step", "m1")

	// Test commands - use the public RegisterCommand API
	i.RegisterCommand("say-hello", cmdSayHello)
	i.RegisterCommand("echo", cmdEcho)
	i.RegisterCommand("count", cmdCount)
	i.RegisterCommand("list", cmdList)

	// Register the Counter foreign type
	feather.RegisterType[*Counter](i, "Counter", feather.TypeDef[*Counter]{
		New: func() *Counter {
			return &Counter{value: 0}
		},
		Methods: map[string]any{
			"get": func(c *Counter) int {
				return c.value
			},
			"set": func(c *Counter, val int) {
				c.value = val
			},
			"incr": func(c *Counter) int {
				c.value++
				return c.value
			},
			"add": func(c *Counter, amount int) int {
				c.value += amount
				return c.value
			},
		},
	})
}

func cmdSayHello(i *feather.Interp, cmd *feather.Obj, args []*feather.Obj) feather.Result {
	fmt.Println("hello")
	return feather.OK("")
}

func cmdEcho(i *feather.Interp, cmd *feather.Obj, args []*feather.Obj) feather.Result {
	for idx, arg := range args {
		if idx > 0 {
			fmt.Print(" ")
		}
		fmt.Print(arg.String())
	}
	fmt.Println()
	return feather.OK("")
}

func cmdCount(i *feather.Interp, cmd *feather.Obj, args []*feather.Obj) feather.Result {
	return feather.OK(fmt.Sprintf("%d", len(args)))
}

func cmdList(i *feather.Interp, cmd *feather.Obj, args []*feather.Obj) feather.Result {
	var parts []string
	for _, arg := range args {
		s := arg.String()
		needsBraces := false
		for _, c := range s {
			if c == ' ' || c == '\t' || c == '\n' || c == '{' || c == '}' {
				needsBraces = true
				break
			}
		}
		if needsBraces {
			parts = append(parts, "{"+s+"}")
		} else if len(s) == 0 {
			parts = append(parts, "{}")
		} else {
			parts = append(parts, s)
		}
	}
	result := ""
	for idx, part := range parts {
		if idx > 0 {
			result += " "
		}
		result += part
	}
	return feather.OK(result)
}

func runREPL(i *feather.Interp) {
	scanner := bufio.NewScanner(os.Stdin)
	var inputBuffer string

	for {
		if inputBuffer == "" {
			fmt.Print("% ")
		} else {
			fmt.Print("> ")
		}

		if !scanner.Scan() {
			break
		}

		line := scanner.Text()
		if inputBuffer != "" {
			inputBuffer += "\n" + line
		} else {
			inputBuffer = line
		}

		parseResult := i.Parse(inputBuffer)
		if parseResult.Status == feather.ParseIncomplete {
			continue
		}

		if parseResult.Status == feather.ParseError {
			fmt.Fprintf(os.Stderr, "error: %s\n", parseResult.Message)
			inputBuffer = ""
			continue
		}

		result, err := i.Eval(inputBuffer)
		if err != nil {
			fmt.Fprintf(os.Stderr, "error: %s\n", err.Error())
		} else if result.String() != "" {
			fmt.Println(result.String())
		}
		inputBuffer = ""
	}

	if err := scanner.Err(); err != nil {
		fmt.Fprintf(os.Stderr, "error reading input: %v\n", err)
	}
}

func runScript(i *feather.Interp) {
	script, err := io.ReadAll(os.Stdin)
	if err != nil {
		fmt.Fprintf(os.Stderr, "error reading script: %v\n", err)
		writeHarnessResult("TCL_ERROR", "", "")
		os.Exit(1)
	}

	parseResult := i.Parse(string(script))
	if parseResult.Status == feather.ParseIncomplete {
		hostParseResult := i.ParseInternal(string(script))
		writeHarnessResult("TCL_OK", hostParseResult.Result, "")
		os.Exit(2)
	}
	if parseResult.Status == feather.ParseError {
		hostParseResult := i.ParseInternal(string(script))
		writeHarnessResult("TCL_ERROR", hostParseResult.Result, parseResult.Message)
		os.Exit(3)
	}

	result, evalErr := i.Eval(string(script))

	if evalErr != nil {
		fmt.Println(evalErr.Error())
		writeHarnessResult("TCL_ERROR", "", evalErr.Error())
		os.Exit(1)
	}

	resultStr := result.String()
	if resultStr != "" {
		fmt.Println(resultStr)
	}
	writeHarnessResult("TCL_OK", resultStr, "")
}

func writeHarnessResult(returnCode string, result string, errorMsg string) {
	if os.Getenv("FEATHER_IN_HARNESS") != "1" {
		return
	}

	f := os.NewFile(3, "harness")
	if f == nil {
		return
	}
	defer f.Close()

	w := bufio.NewWriter(f)
	fmt.Fprintf(w, "return: %s\n", returnCode)
	if result != "" {
		fmt.Fprintf(w, "result: %s\n", result)
	}
	if errorMsg != "" {
		fmt.Fprintf(w, "error: %s\n", errorMsg)
	}
	w.Flush()
}

func runBenchmarkMode() {
	// Read benchmarks from stdin
	var benchmarks []Benchmark
	decoder := json.NewDecoder(os.Stdin)
	if err := decoder.Decode(&benchmarks); err != nil {
		fmt.Fprintf(os.Stderr, "error reading benchmarks: %v\n", err)
		os.Exit(1)
	}

	// Create interpreter once for all benchmarks
	i := feather.New()
	defer i.Close()

	// Register test commands (in case benchmarks use them)
	registerTestCommands(i)

	// Open harness channel
	f := os.NewFile(3, "harness")
	if f == nil {
		fmt.Fprintf(os.Stderr, "error: harness channel not available\n")
		os.Exit(1)
	}
	defer f.Close()
	w := bufio.NewWriter(f)

	// Run each benchmark
	for _, b := range benchmarks {
		result := runSingleBenchmark(i, b)

		// Write result as JSON
		resultJSON, _ := json.Marshal(result)
		fmt.Fprintf(w, "%s\n", resultJSON)
		w.Flush()
	}
}

func runSingleBenchmark(i *feather.Interp, b Benchmark) BenchmarkResult {
	result := BenchmarkResult{
		Benchmark: b,
		Success:   true,
	}

	// Run setup if provided
	if b.Setup != "" {
		_, err := i.Eval(b.Setup)
		if err != nil {
			result.Success = false
			result.Error = fmt.Sprintf("setup failed: %s", err.Error())
			return result
		}
	}

	// Warmup iterations
	for w := 0; w < b.Warmup; w++ {
		_, err := i.Eval(b.Script)
		if err != nil {
			result.Success = false
			result.Error = fmt.Sprintf("warmup failed: %s", err.Error())
			return result
		}
	}

	// Measured iterations
	var totalTime time.Duration
	var minTime time.Duration
	var maxTime time.Duration

	for iter := 0; iter < b.Iterations; iter++ {
		start := time.Now()
		_, err := i.Eval(b.Script)
		elapsed := time.Since(start)

		if err != nil {
			result.Success = false
			result.Error = fmt.Sprintf("iteration %d failed: %s", iter, err.Error())
			return result
		}

		totalTime += elapsed
		if iter == 0 || elapsed < minTime {
			minTime = elapsed
		}
		if elapsed > maxTime {
			maxTime = elapsed
		}
		result.Iterations++
	}

	// Calculate statistics
	result.TotalTime = totalTime
	result.AvgTime = totalTime / time.Duration(b.Iterations)
	result.MinTime = minTime
	result.MaxTime = maxTime
	if result.AvgTime > 0 {
		result.OpsPerSecond = float64(time.Second) / float64(result.AvgTime)
	}

	return result
}
