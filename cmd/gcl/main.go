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
		errorMsg := parseErrorMessage(parseResult.Result)
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

// parseErrorMessage extracts the error message from a parse error result.
// The result format is: {ERROR start_offset end_offset {message}}
func parseErrorMessage(result string) string {
	if strings.HasPrefix(result, "{ERROR") {
		// Extract the message (last element in the list)
		// Format: {ERROR 5 8 {extra characters after close-brace}}
		trimmed := strings.TrimPrefix(result, "{")
		trimmed = strings.TrimSuffix(trimmed, "}")
		parts := strings.SplitN(trimmed, " ", 4)
		if len(parts) >= 4 {
			msg := parts[3]
			// Strip braces if present
			msg = strings.TrimPrefix(msg, "{")
			msg = strings.TrimSuffix(msg, "}")
			return msg
		}
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
