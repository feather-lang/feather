package main

import (
	"bufio"
	"fmt"
	"io"
	"os"

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

func writeHarnessResult(code string, value string) {
	if os.Getenv("TCLC_IN_HARNESS") != "1" {
		return
	}

	f := os.NewFile(3, "harness")
	if f == nil {
		return
	}
	defer f.Close()

	w := bufio.NewWriter(f)
	fmt.Fprintf(w, "result: %s\n", code)
	if value != "" {
		fmt.Fprintf(w, "value: %s\n", value)
	}
	w.Flush()
}
