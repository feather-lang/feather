package main

import (
	"bufio"
	"fmt"
	"io"
	"os"
	"strings"

	"github.com/dhamidi/tclc/interp"
	defaults "github.com/dhamidi/tclc/interp/default"
)

func main() {
	host := defaults.NewHost()

	script, err := io.ReadAll(os.Stdin)
	if err != nil {
		fmt.Fprintf(os.Stderr, "error reading script: %v\n", err)
		writeHarnessResult("TCL_ERROR", "", "")
		os.Exit(1)
	}

	// First, parse the script to check for incomplete input
	parseResult := host.Parse(string(script))
	if parseResult.Status == interp.ParseIncomplete {
		writeHarnessResult("TCL_OK", parseResult.Result, "")
		os.Exit(2) // Exit code 2 signals incomplete input
	}
	if parseResult.Status == interp.ParseError {
		errorMsg := parseErrorMessage(parseResult.Result, string(script))
		writeHarnessResult("TCL_ERROR", parseResult.Result, errorMsg)
		os.Exit(3) // Exit code 3 signals parse error
	}

	// Parse succeeded, now evaluate
	result, err := host.Eval(string(script))

	if err != nil {
		fmt.Fprintln(os.Stderr, err.Error())
		writeHarnessResult("TCL_ERROR", "", err.Error())
		os.Exit(1)
	}

	if result != "" {
		fmt.Println(result)
	}
	writeHarnessResult("TCL_OK", result, "")
}

// parseErrorMessage converts a parse error result to a human-readable message.
func parseErrorMessage(result string, script string) string {
	if strings.HasPrefix(result, "{ERROR") {
		// Extract start position from {ERROR start end}
		var start int
		fmt.Sscanf(result, "{ERROR %d", &start)
		if start >= 0 && start < len(script) {
			if script[start] == '"' {
				return "extra characters after close-quote"
			}
		}
		return "extra characters after close-brace"
	}
	return "parse error"
}

func writeHarnessResult(returnCode string, result string, errorMsg string) {
	if os.Getenv("TCLC_IN_HARNESS") != "1" {
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
