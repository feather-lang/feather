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

	// M1 commands
	h.Register("say-hello", cmdSayHello)

	return h
}

func cmdSayHello(i *interp.Interp, cmd interp.TclObj, args []interp.TclObj) interp.TclResult {
	fmt.Println("hello")
	i.SetResultString("")
	return interp.ResultOK
}
