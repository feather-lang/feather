package main

import (
	"bufio"
	"fmt"
	"io"
	"os"

	"github.com/dhamidi/tclc/interp"
	defaults "github.com/dhamidi/tclc/interp/default"
)

func main() {
	host := defaults.NewHost()

	script, err := io.ReadAll(os.Stdin)
	if err != nil {
		fmt.Fprintf(os.Stderr, "error reading script: %v\n", err)
		writeHarnessResult("TCL_ERROR", "")
		os.Exit(1)
	}

	// First, parse the script to check for incomplete input
	parseResult := host.Parse(string(script))
	if parseResult.Status == interp.ParseIncomplete {
		writeHarnessResult("TCL_OK", parseResult.Result)
		os.Exit(2) // Exit code 2 signals incomplete input
	}
	if parseResult.Status == interp.ParseError {
		fmt.Fprintln(os.Stderr, parseResult.Result)
		writeHarnessResult("TCL_ERROR", parseResult.Result)
		os.Exit(1)
	}

	// Parse succeeded, now evaluate
	result, err := host.Eval(string(script))

	if err != nil {
		fmt.Fprintln(os.Stderr, err.Error())
		writeHarnessResult("TCL_ERROR", err.Error())
		os.Exit(1)
	}

	if result != "" {
		fmt.Println(result)
	}
	writeHarnessResult("TCL_OK", result)
}

func writeHarnessResult(returnCode string, result string) {
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
	w.Flush()
}
