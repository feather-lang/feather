// Package defaults provides pre-configured interpreters for different milestones.
package defaults

import (
	"fmt"

	"github.com/dhamidi/tclc/interp"
)

// NewHost creates a Host configured for milestone 1.
// M1 tests basic command invocation from the host.
func NewHost() *interp.Host {
	h := interp.NewHost()

	// Set milestone variables
	h.Interp.SetVar("milestone", "m1")
	h.Interp.SetVar("current-step", "m1")

	// M1 commands
	h.Register("say-hello", cmdSayHello)
	h.Register("echo", cmdEcho)
	h.Register("count", cmdCount)
	h.Register("list", cmdList)

	return h
}

func cmdSayHello(i *interp.Interp, cmd interp.TclObj, args []interp.TclObj) interp.TclResult {
	fmt.Println("hello")
	i.SetResultString("")
	return interp.ResultOK
}

func cmdEcho(i *interp.Interp, cmd interp.TclObj, args []interp.TclObj) interp.TclResult {
	// Print all arguments separated by spaces
	for idx, arg := range args {
		if idx > 0 {
			fmt.Print(" ")
		}
		fmt.Print(i.GetString(arg))
	}
	fmt.Println()
	i.SetResultString("")
	return interp.ResultOK
}

func cmdCount(i *interp.Interp, cmd interp.TclObj, args []interp.TclObj) interp.TclResult {
	// Return the number of arguments received
	i.SetResultString(fmt.Sprintf("%d", len(args)))
	return interp.ResultOK
}

func cmdList(i *interp.Interp, cmd interp.TclObj, args []interp.TclObj) interp.TclResult {
	// Return arguments as a properly formatted TCL list
	var parts []string
	for _, arg := range args {
		s := i.GetString(arg)
		// Check if the string needs bracing (contains spaces or special chars)
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
	i.SetResultString(result)
	return interp.ResultOK
}
