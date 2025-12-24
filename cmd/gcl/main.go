package main

import (
	"bufio"
	"fmt"
	"io"
	"os"

	"github.com/feather-lang/feather/interp"
	defaults "github.com/feather-lang/feather/interp/default"
)

func main() {
	host := defaults.NewHost()

	// Check if stdin is a TTY
	stat, _ := os.Stdin.Stat()
	if (stat.Mode() & os.ModeCharDevice) != 0 {
		runREPL(host)
		return
	}

	runScript(host)
}

func runREPL(host *interp.Host) {
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
		parseResult := host.Parse(inputBuffer)
		if parseResult.Status == interp.ParseIncomplete {
			continue // Need more input
		}

		if parseResult.Status == interp.ParseError {
			fmt.Fprintf(os.Stderr, "error: %s\n", parseResult.ErrorMessage)
			inputBuffer = ""
			continue
		}

		// Evaluate the complete input
		result, err := host.Eval(inputBuffer)
		if err != nil {
			fmt.Fprintf(os.Stderr, "error: %s\n", err.Error())
		} else if result != "" {
			fmt.Println(result)
		}
		inputBuffer = ""
	}

	if err := scanner.Err(); err != nil {
		fmt.Fprintf(os.Stderr, "error reading input: %v\n", err)
	}
}

func runScript(host *interp.Host) {
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
		writeHarnessResult("TCL_ERROR", parseResult.Result, parseResult.ErrorMessage)
		os.Exit(3) // Exit code 3 signals parse error
	}

	// Parse succeeded, now evaluate
	result, err := host.Eval(string(script))

	if err != nil {
		fmt.Println(err.Error())
		writeHarnessResult("TCL_ERROR", "", err.Error())
		os.Exit(1)
	}

	if result != "" {
		fmt.Println(result)
	}
	writeHarnessResult("TCL_OK", result, "")
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
