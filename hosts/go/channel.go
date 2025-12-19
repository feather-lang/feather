package main

/*
#include <stddef.h>
#include <stdint.h>
*/
import "C"
import (
	"bufio"
	"io"
	"os"
	"runtime/cgo"
	"strings"
	"sync"
	"unsafe"
)

// Channel represents an I/O channel
type Channel struct {
	handle   cgo.Handle
	name     string
	file     *os.File
	reader   *bufio.Reader
	writer   *bufio.Writer
	readable bool
	writable bool
	isStd    bool // Standard stream (don't close)
	eof      bool
}

// Handle returns the stable handle for this Channel
func (ch *Channel) Handle() uintptr {
	if ch == nil {
		return 0
	}
	return uintptr(ch.handle)
}

// getChan retrieves a Channel by handle using cgo.Handle
func getChan(h uintptr) *Channel {
	if h == 0 {
		return nil
	}
	return cgo.Handle(h).Value().(*Channel)
}

// freeChan frees the Channel's handle
func freeChan(ch *Channel) {
	if ch == nil {
		return
	}
	if ch.handle != 0 {
		ch.handle.Delete()
		ch.handle = 0
	}
}

// Standard channel handles (fixed values, initialized once)
var (
	stdinChan    *Channel
	stdoutChan   *Channel
	stderrChan   *Channel
	stdinHandle  uintptr
	stdoutHandle uintptr
	stderrHandle uintptr
	stdOnce      sync.Once
)

func initStdChannels() {
	stdOnce.Do(func() {
		stdinChan = &Channel{
			name:     "stdin",
			file:     os.Stdin,
			reader:   bufio.NewReader(os.Stdin),
			readable: true,
			isStd:    true,
		}
		stdinChan.handle = cgo.NewHandle(stdinChan)
		stdinHandle = uintptr(stdinChan.handle)

		stdoutChan = &Channel{
			name:     "stdout",
			file:     os.Stdout,
			writer:   bufio.NewWriter(os.Stdout),
			writable: true,
			isStd:    true,
		}
		stdoutChan.handle = cgo.NewHandle(stdoutChan)
		stdoutHandle = uintptr(stdoutChan.handle)

		stderrChan = &Channel{
			name:     "stderr",
			file:     os.Stderr,
			writer:   bufio.NewWriter(os.Stderr),
			writable: true,
			isStd:    true,
		}
		stderrChan.handle = cgo.NewHandle(stderrChan)
		stderrHandle = uintptr(stderrChan.handle)
	})
}

// OpenChannel opens a file channel
func OpenChannel(name, mode string) (*Channel, error) {
	var flags int
	var readable, writable bool

	switch {
	case strings.Contains(mode, "r") && strings.Contains(mode, "+"):
		flags = os.O_RDWR
		readable = true
		writable = true
	case strings.Contains(mode, "r"):
		flags = os.O_RDONLY
		readable = true
	case strings.Contains(mode, "w"):
		flags = os.O_WRONLY | os.O_CREATE | os.O_TRUNC
		writable = true
	case strings.Contains(mode, "a"):
		flags = os.O_WRONLY | os.O_CREATE | os.O_APPEND
		writable = true
	default:
		flags = os.O_RDONLY
		readable = true
	}

	file, err := os.OpenFile(name, flags, 0644)
	if err != nil {
		return nil, err
	}

	ch := &Channel{
		name:     name,
		file:     file,
		readable: readable,
		writable: writable,
	}

	if readable {
		ch.reader = bufio.NewReader(file)
	}
	if writable {
		ch.writer = bufio.NewWriter(file)
	}

	ch.handle = cgo.NewHandle(ch)
	return ch, nil
}

// Close closes the channel
func (ch *Channel) Close() error {
	if ch.isStd {
		return nil
	}
	if ch.writer != nil {
		ch.writer.Flush()
	}
	return ch.file.Close()
}

// Read reads up to len(buf) bytes
func (ch *Channel) Read(buf []byte) (int, error) {
	if !ch.readable || ch.reader == nil {
		return 0, io.EOF
	}
	n, err := ch.reader.Read(buf)
	if err == io.EOF {
		ch.eof = true
	}
	return n, err
}

// Write writes buf to the channel
func (ch *Channel) Write(buf []byte) (int, error) {
	if !ch.writable {
		return 0, nil
	}
	// For standard streams, write directly to bypass buffering issues
	if ch.isStd {
		return ch.file.Write(buf)
	}
	if ch.writer == nil {
		return 0, nil
	}
	return ch.writer.Write(buf)
}

// Gets reads a line (without newline)
func (ch *Channel) Gets() (string, bool) {
	if !ch.readable || ch.reader == nil {
		return "", true
	}

	line, err := ch.reader.ReadString('\n')
	if err == io.EOF {
		ch.eof = true
		if len(line) == 0 {
			return "", true
		}
	}

	// Remove trailing newline
	line = strings.TrimSuffix(line, "\n")
	line = strings.TrimSuffix(line, "\r")

	return line, false
}

// Flush flushes the write buffer
func (ch *Channel) Flush() error {
	if ch.writer != nil {
		return ch.writer.Flush()
	}
	return nil
}

// Seek seeks to position
func (ch *Channel) Seek(offset int64, whence int) error {
	if ch.writer != nil {
		ch.writer.Flush()
	}
	_, err := ch.file.Seek(offset, whence)
	if ch.reader != nil {
		ch.reader.Reset(ch.file)
	}
	return err
}

// Tell returns current position
func (ch *Channel) Tell() int64 {
	pos, _ := ch.file.Seek(0, io.SeekCurrent)
	return pos
}

// Eof returns true if at end of file
func (ch *Channel) Eof() bool {
	return ch.eof
}

// CGO exports for channel operations

//export goChanOpen
func goChanOpen(ctxHandle uintptr, name *C.char, mode *C.char) uintptr {
	goName := C.GoString(name)
	goMode := C.GoString(mode)

	ch, err := OpenChannel(goName, goMode)
	if err != nil {
		return 0
	}

	return ch.Handle()
}

//export goChanClose
func goChanClose(ctxHandle uintptr, chanHandle uintptr) {
	ch := getChan(chanHandle)
	if ch == nil {
		return
	}

	ch.Close()
	freeChan(ch)
}

//export goChanStdin
func goChanStdin(ctxHandle uintptr) uintptr {
	initStdChannels()
	return stdinHandle // Always return the same fixed handle
}

//export goChanStdout
func goChanStdout(ctxHandle uintptr) uintptr {
	initStdChannels()
	return stdoutHandle // Always return the same fixed handle
}

//export goChanStderr
func goChanStderr(ctxHandle uintptr) uintptr {
	initStdChannels()
	return stderrHandle // Always return the same fixed handle
}

//export goChanRead
func goChanRead(chanHandle uintptr, buf *C.char, length C.size_t) C.int {
	ch := getChan(chanHandle)
	if ch == nil {
		return -1
	}

	goBuf := unsafe.Slice((*byte)(unsafe.Pointer(buf)), int(length))
	n, err := ch.Read(goBuf)
	if err != nil && err != io.EOF {
		return -1
	}
	return C.int(n)
}

//export goChanWrite
func goChanWrite(chanHandle uintptr, buf *C.char, length C.size_t) C.int {
	ch := getChan(chanHandle)
	if ch == nil {
		return -1
	}

	goBuf := C.GoBytes(unsafe.Pointer(buf), C.int(length))
	n, err := ch.Write(goBuf)
	if err != nil {
		return -1
	}
	return C.int(n)
}

//export goChanGets
func goChanGets(chanHandle uintptr, eofOut *C.int) uintptr {
	ch := getChan(chanHandle)
	if ch == nil {
		if eofOut != nil {
			*eofOut = 1
		}
		return 0
	}

	line, eof := ch.Gets()
	if eof && len(line) == 0 {
		if eofOut != nil {
			*eofOut = 1
		}
		return 0
	}

	if eofOut != nil {
		if eof {
			*eofOut = 1
		} else {
			*eofOut = 0
		}
	}

	obj := NewString(line)
	return obj.Handle()
}

//export goChanFlush
func goChanFlush(chanHandle uintptr) C.int {
	ch := getChan(chanHandle)
	if ch == nil {
		return -1
	}

	if err := ch.Flush(); err != nil {
		return -1
	}
	return 0
}

//export goChanSeek
func goChanSeek(chanHandle uintptr, offset C.int64_t, whence C.int) C.int {
	ch := getChan(chanHandle)
	if ch == nil {
		return -1
	}

	if err := ch.Seek(int64(offset), int(whence)); err != nil {
		return -1
	}
	return 0
}

//export goChanTell
func goChanTell(chanHandle uintptr) C.int64_t {
	ch := getChan(chanHandle)
	if ch == nil {
		return -1
	}
	return C.int64_t(ch.Tell())
}

//export goChanEof
func goChanEof(chanHandle uintptr) C.int {
	ch := getChan(chanHandle)
	if ch == nil {
		return 1
	}
	if ch.Eof() {
		return 1
	}
	return 0
}

//export goChanBlocked
func goChanBlocked(chanHandle uintptr) C.int {
	// File channels are never blocked
	return 0
}

//export goChanConfigure
func goChanConfigure(chanHandle uintptr, opt *C.char, valHandle uintptr) C.int {
	// Stub for now
	return 0
}

//export goChanCget
func goChanCget(chanHandle uintptr, opt *C.char) uintptr {
	obj := NewString("")
	return obj.Handle()
}

//export goChanNames
func goChanNames(ctxHandle uintptr, pattern *C.char) uintptr {
	obj := NewString("stdin stdout stderr")
	return obj.Handle()
}

//export goChanShare
func goChanShare(fromCtx, toCtx uintptr, chanHandle uintptr) {
	// Stub
}

//export goChanTransfer
func goChanTransfer(fromCtx, toCtx uintptr, chanHandle uintptr) {
	// Stub
}
