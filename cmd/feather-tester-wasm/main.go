// feather-tester-wasm is a WASM version of the test harness interpreter.
// It relies on a JavaScript host to provide the TCL interpreter functionality.
package main

import (
	"os"
	"unsafe"
)

// WASM imports from JavaScript host
// These bridge to the feather.js module

//go:wasmimport feather create_interp
func createInterp() uint32

//go:wasmimport feather destroy_interp
func destroyInterp(id uint32)

//go:wasmimport feather eval
func featherEval(interpId uint32, scriptPtr *byte, scriptLen uint32, resultPtr *byte, resultLen *uint32) uint32

//go:wasmimport feather parse
func featherParse(interpId uint32, scriptPtr *byte, scriptLen uint32) uint32

//go:wasmimport feather register_command
func registerCommand(interpId uint32, namePtr *byte, nameLen uint32, callbackId uint32)

//go:wasmimport feather set_var
func setVar(interpId uint32, namePtr *byte, nameLen uint32, valuePtr *byte, valueLen uint32)

//go:wasmimport feather write_stdout
func writeStdout(ptr *byte, len uint32)

//go:wasmimport feather write_stderr
func writeStderr(ptr *byte, len uint32)

//go:wasmimport feather write_fd
func writeFd(fd uint32, ptr *byte, len uint32)

//go:wasmimport feather read_stdin
func readStdin(bufPtr *byte, bufLen uint32) uint32

//go:wasmimport feather is_tty
func isTty() uint32

//go:wasmimport feather get_env
func getEnv(namePtr *byte, nameLen uint32, valuePtr *byte, valueLen *uint32) uint32

// Command callback - called from JavaScript when a registered command is invoked
// The callbackId identifies which command, args is the space-separated arguments

//go:export feather_command_callback
func commandCallback(callbackId uint32, argsPtr *byte, argsLen uint32, resultPtr *byte, resultLen *uint32) uint32 {
	args := ptrToString(argsPtr, argsLen)
	result, isError := handleCommand(callbackId, args)
	copyStringToPtr(result, resultPtr, resultLen)
	if isError {
		return 1 // TCL_ERROR
	}
	return 0 // TCL_OK
}

// Command handlers
var commandHandlers = map[uint32]func(args string) (string, bool){
	1: cmdSayHello,
	2: cmdEcho,
	3: cmdCount,
	4: cmdList,
}

func handleCommand(id uint32, args string) (string, bool) {
	if handler, ok := commandHandlers[id]; ok {
		return handler(args)
	}
	return "unknown command", true
}

func cmdSayHello(args string) (string, bool) {
	println("hello")
	return "", false
}

func cmdEcho(args string) (string, bool) {
	println(args)
	return "", false
}

func cmdCount(args string) (string, bool) {
	// Count space-separated arguments
	if args == "" {
		return "0", false
	}
	count := 1
	for i := 0; i < len(args); i++ {
		if args[i] == ' ' {
			count++
		}
	}
	return intToString(count), false
}

func cmdList(args string) (string, bool) {
	// Simple list implementation - just return the args as-is for now
	// A proper implementation would handle quoting
	return args, false
}

// Helper functions
func println(s string) {
	if len(s) > 0 {
		writeStdout(unsafe.StringData(s), uint32(len(s)))
	}
	writeStdout(unsafe.StringData("\n"), 1)
}

func eprintln(s string) {
	if len(s) > 0 {
		writeStderr(unsafe.StringData(s), uint32(len(s)))
	}
	writeStderr(unsafe.StringData("\n"), 1)
}

func ptrToString(ptr *byte, len uint32) string {
	if ptr == nil || len == 0 {
		return ""
	}
	return unsafe.String(ptr, len)
}

func copyStringToPtr(s string, ptr *byte, lenPtr *uint32) {
	if ptr == nil || lenPtr == nil {
		return
	}
	*lenPtr = uint32(len(s))
	if len(s) > 0 {
		copy(unsafe.Slice(ptr, len(s)), s)
	}
}

func intToString(n int) string {
	if n == 0 {
		return "0"
	}
	neg := n < 0
	if neg {
		n = -n
	}
	var buf [20]byte
	i := len(buf)
	for n > 0 {
		i--
		buf[i] = byte('0' + n%10)
		n /= 10
	}
	if neg {
		i--
		buf[i] = '-'
	}
	return string(buf[i:])
}

// Global state
var (
	interpId uint32
	resultBuffer [4096]byte
)

func main() {
	// Create interpreter
	interpId = createInterp()
	defer destroyInterp(interpId)

	// Register test commands
	registerTestCommands()

	// Check if stdin is a TTY
	if isTty() != 0 {
		runREPL()
		return
	}

	runScript()
}

func registerTestCommands() {
	// Set milestone variables
	name := "milestone"
	value := "m1"
	setVar(interpId, unsafe.StringData(name), uint32(len(name)),
		unsafe.StringData(value), uint32(len(value)))

	name = "current-step"
	setVar(interpId, unsafe.StringData(name), uint32(len(name)),
		unsafe.StringData(value), uint32(len(value)))

	// Register commands
	registerCmd("say-hello", 1)
	registerCmd("echo", 2)
	registerCmd("count", 3)
	registerCmd("list", 4)
}

func registerCmd(name string, id uint32) {
	registerCommand(interpId, unsafe.StringData(name), uint32(len(name)), id)
}

func runREPL() {
	var inputBuffer [4096]byte
	var input string

	for {
		// Print prompt
		if input == "" {
			print("% ")
		} else {
			print("> ")
		}

		// Read line
		n := readStdin(&inputBuffer[0], uint32(len(inputBuffer)))
		if n == 0 {
			break
		}

		line := string(inputBuffer[:n])
		if input != "" {
			input = input + "\n" + line
		} else {
			input = line
		}

		// Try to parse
		status := featherParse(interpId, unsafe.StringData(input), uint32(len(input)))
		if status == 1 { // INCOMPLETE
			continue
		}

		if status == 2 { // ERROR
			eprintln("error: parse error")
			input = ""
			continue
		}

		// Evaluate
		var resultLen uint32
		result := featherEval(interpId, unsafe.StringData(input), uint32(len(input)),
			&resultBuffer[0], &resultLen)

		if result != 0 {
			eprintln("error: " + string(resultBuffer[:resultLen]))
		} else if resultLen > 0 {
			println(string(resultBuffer[:resultLen]))
		}

		input = ""
	}
}

func runScript() {
	// Read entire script from stdin
	var scriptBuffer [65536]byte
	var totalRead uint32

	for {
		n := readStdin(&scriptBuffer[totalRead], uint32(len(scriptBuffer))-totalRead)
		if n == 0 {
			break
		}
		totalRead += n
	}

	script := string(scriptBuffer[:totalRead])

	// Parse first
	status := featherParse(interpId, unsafe.StringData(script), uint32(len(script)))
	if status == 1 { // INCOMPLETE
		writeHarnessResult("TCL_OK", "", "")
		os.Exit(2)
	}
	if status == 2 { // ERROR
		writeHarnessResult("TCL_ERROR", "", "parse error")
		os.Exit(3)
	}

	// Evaluate
	var resultLen uint32
	result := featherEval(interpId, unsafe.StringData(script), uint32(len(script)),
		&resultBuffer[0], &resultLen)

	resultStr := string(resultBuffer[:resultLen])
	if result != 0 {
		println(resultStr)
		writeHarnessResult("TCL_ERROR", "", resultStr)
		os.Exit(1)
	}

	if resultLen > 0 {
		println(resultStr)
	}
	writeHarnessResult("TCL_OK", resultStr, "")
}

func writeHarnessResult(returnCode, result, errorMsg string) {
	// Check environment variable
	var envValue [2]byte
	var envLen uint32
	envName := "FEATHER_IN_HARNESS"
	if getEnv(unsafe.StringData(envName), uint32(len(envName)), &envValue[0], &envLen) == 0 {
		return
	}
	if envLen != 1 || envValue[0] != '1' {
		return
	}

	// Write to fd 3
	msg := "return: " + returnCode + "\n"
	writeFd(3, unsafe.StringData(msg), uint32(len(msg)))

	if result != "" {
		msg = "result: " + result + "\n"
		writeFd(3, unsafe.StringData(msg), uint32(len(msg)))
	}

	if errorMsg != "" {
		msg = "error: " + errorMsg + "\n"
		writeFd(3, unsafe.StringData(msg), uint32(len(msg)))
	}
}

func print(s string) {
	if len(s) > 0 {
		writeStdout(unsafe.StringData(s), uint32(len(s)))
	}
}
