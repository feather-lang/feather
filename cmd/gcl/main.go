package main

import (
	"bufio"
	"fmt"
	"io"
	"os"

	"github.com/feather-lang/feather"
	"github.com/feather-lang/feather/interp"
)

func main() {
	i := feather.New()
	defer i.Close()

	// Check if stdin is a TTY
	stat, _ := os.Stdin.Stat()
	if (stat.Mode() & os.ModeCharDevice) != 0 {
		runREPL(i)
		return
	}

	runScript(i)
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

		// Check if input is complete
		parseResult := i.Parse(inputBuffer)
		if parseResult.Status == feather.ParseIncomplete {
			continue // Need more input
		}

		if parseResult.Status == feather.ParseError {
			fmt.Fprintf(os.Stderr, "error: %s\n", parseResult.Message)
			inputBuffer = ""
			continue
		}

		// Evaluate the complete input
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

	// First, parse the script to check for incomplete input
	parseResult := i.Parse(string(script))
	if parseResult.Status == feather.ParseIncomplete {
		// Need to get the parse result from Internal() for harness compatibility
		hostParseResult := i.Internal().Parse(string(script))
		writeHarnessResult("TCL_OK", hostParseResult.Result, "")
		os.Exit(2) // Exit code 2 signals incomplete input
	}
	if parseResult.Status == feather.ParseError {
		hostParseResult := i.Internal().Parse(string(script))
		writeHarnessResult("TCL_ERROR", hostParseResult.Result, parseResult.Message)
		os.Exit(3) // Exit code 3 signals parse error
	}

	// Parse succeeded, now evaluate
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

// Ensure we still use interp for the harness protocol
var _ = interp.ParseOK
