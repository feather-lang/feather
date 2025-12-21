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
