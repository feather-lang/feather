//go:build windows

package main

import (
	"os"
)

func setupResizeSignal() (chan os.Signal, func()) {
	// Windows doesn't have SIGWINCH; return a dummy channel that never receives
	return make(chan os.Signal, 1), func() {}
}
