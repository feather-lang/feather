//go:build !windows

package main

import (
	"os"
	"os/signal"
	"syscall"
)

func setupResizeSignal() (chan os.Signal, func()) {
	sigwinch := make(chan os.Signal, 1)
	signal.Notify(sigwinch, syscall.SIGWINCH)
	return sigwinch, func() { signal.Stop(sigwinch) }
}
