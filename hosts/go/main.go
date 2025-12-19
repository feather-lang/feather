package main

/*
#cgo CFLAGS: -I../../core
#cgo LDFLAGS: -lm
#include "../../core/tclc.h"
#include "../../core/lexer.c"
#include "../../core/parser.c"
#include "../../core/subst.c"
#include "../../core/eval.c"
#include "../../core/builtins.c"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// External: get the Go host table
extern const TclHost* tclGetGoHost(void);

// External: interpreter management and eval (from core)
extern TclInterp* tclInterpNew(const TclHost* host, void* hostCtx);
extern void tclInterpFree(TclInterp* interp);
extern TclResult tclEvalScript(TclInterp* interp, const char* script, size_t len);

// Helper to call function pointers (CGO can't call them directly)
static void* callInterpContextNew(const TclHost* host, void* parent, int safe) {
    return host->interpContextNew(parent, safe);
}

static void callInterpContextFree(const TclHost* host, void* ctx) {
    host->interpContextFree(ctx);
}

static const char* callGetStringPtr(const TclHost* host, TclObj* obj, size_t* lenOut) {
    return host->getStringPtr(obj, lenOut);
}
*/
import "C"
import (
	"fmt"
	"io"
	"os"
	"unsafe"
)

func main() {
	var filename string
	var script []byte
	var err error

	// Parse arguments
	if len(os.Args) > 1 {
		filename = os.Args[1]
		script, err = os.ReadFile(filename)
		if err != nil {
			fmt.Fprintf(os.Stderr, "couldn't read file %q: %v\n", filename, err)
			os.Exit(1)
		}
	} else {
		// Read from stdin
		script, err = io.ReadAll(os.Stdin)
		if err != nil {
			fmt.Fprintf(os.Stderr, "error reading from stdin: %v\n", err)
			os.Exit(1)
		}
	}

	// Get host and create interpreter
	host := C.tclGetGoHost()

	hostCtx := C.callInterpContextNew(host, nil, 0)
	if hostCtx == nil {
		fmt.Fprintf(os.Stderr, "failed to create host context\n")
		os.Exit(1)
	}
	defer C.callInterpContextFree(host, hostCtx)

	interp := C.tclInterpNew(host, hostCtx)
	if interp == nil {
		fmt.Fprintf(os.Stderr, "failed to create interpreter\n")
		os.Exit(1)
	}
	defer C.tclInterpFree(interp)

	// Set script file for error reporting
	if filename != "" {
		cFilename := C.CString(filename)
		defer C.free(unsafe.Pointer(cFilename))
		interp.scriptFile = cFilename
	}

	// Evaluate script
	cScript := C.CString(string(script))
	defer C.free(unsafe.Pointer(cScript))

	result := C.tclEvalScript(interp, cScript, C.size_t(len(script)))

	// Report errors
	exitCode := 0
	if result == C.TCL_ERROR {
		// Print error message
		if interp.result != nil {
			var msgLen C.size_t
			msg := C.callGetStringPtr(host, interp.result, &msgLen)
			if msg != nil {
				fmt.Fprintf(os.Stderr, "%s\n", C.GoStringN(msg, C.int(msgLen)))
			}
		}

		// Print error info (stack trace)
		if interp.errorInfo != nil {
			var infoLen C.size_t
			info := C.callGetStringPtr(host, interp.errorInfo, &infoLen)
			if info != nil {
				fmt.Fprintf(os.Stderr, "%s\n", C.GoStringN(info, C.int(infoLen)))
			}
		}

		exitCode = 1
	}

	os.Exit(exitCode)
}
