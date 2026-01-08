package main

import (
	"fmt"
	"io"
	"os"
	"os/signal"
	"strings"
	"syscall"

	"github.com/feather-lang/feather"
	"golang.org/x/term"
)

// CompletionCandidate represents a single completion suggestion.
type CompletionCandidate struct {
	Text string
	Type string
	Help string
	Name string // for arg-placeholder type
}

// keyResult holds a key press result
type keyResult struct {
	key string
	err error
}

// LineEditor provides an interactive line editor with completion support.
type LineEditor struct {
	interp   *feather.Interp
	oldState *term.State
	fd       int

	// Current line state
	line   []rune
	cursor int

	// Completion state
	completions    []CompletionCandidate
	selected       int
	showPopup      bool
	popupLineCount int // Number of popup lines currently displayed

	// Multi-line input accumulator
	inputBuffer string

	// Pending input bytes (for when we read multiple bytes at once)
	pendingInput []byte

	// Persistent key reader
	keyChan   chan keyResult
	readerRunning bool
}

// NewLineEditor creates a new line editor for the given interpreter.
func NewLineEditor(interp *feather.Interp) *LineEditor {
	return &LineEditor{
		interp: interp,
		fd:     int(os.Stdin.Fd()),
	}
}

// enterRawMode puts the terminal in raw mode.
func (e *LineEditor) enterRawMode() error {
	oldState, err := term.MakeRaw(e.fd)
	if err != nil {
		return err
	}
	e.oldState = oldState
	return nil
}

// exitRawMode restores the terminal to its original state.
func (e *LineEditor) exitRawMode() {
	if e.oldState != nil {
		term.Restore(e.fd, e.oldState)
		e.oldState = nil
	}
}

// getTerminalWidth returns the terminal width or a default.
func (e *LineEditor) getTerminalWidth() int {
	width, _, err := term.GetSize(e.fd)
	if err != nil || width <= 0 {
		return 80
	}
	// Safety margin - some terminals report width including scrollbar area
	if width > 80 {
		return width - 1
	}
	return width
}

// debugLog writes debug info to stderr
var debugEnabled = os.Getenv("DEBUG_KEYS") == "1"

func debugLog(format string, args ...interface{}) {
	if debugEnabled {
		fmt.Fprintf(os.Stderr, format+"\n", args...)
	}
}

// readByte reads a single byte, using pending buffer first
func (e *LineEditor) readByte() (byte, error) {
	if len(e.pendingInput) > 0 {
		b := e.pendingInput[0]
		e.pendingInput = e.pendingInput[1:]
		return b, nil
	}

	buf := make([]byte, 32)
	n, err := os.Stdin.Read(buf)
	if err != nil {
		return 0, err
	}
	if n == 0 {
		return 0, io.EOF
	}

	debugLog("readByte: read %d bytes: %v %q", n, buf[:n], string(buf[:n]))

	// Store all but first byte in pending buffer
	if n > 1 {
		e.pendingInput = append(e.pendingInput, buf[1:n]...)
	}
	return buf[0], nil
}

// skipToTerminator skips bytes until we find a CSI sequence terminator (0x40-0x7E)
func (e *LineEditor) skipToTerminator() {
	for {
		b, err := e.readByte()
		if err != nil {
			return
		}
		// CSI terminators are in range 0x40-0x7E (@ to ~)
		if b >= 0x40 && b <= 0x7E {
			return
		}
	}
}

// readKey reads a single key press, handling escape sequences.
func (e *LineEditor) readKey() (key string, err error) {
	ch, err := e.readByte()
	if err != nil {
		return "", err
	}

	// Handle escape sequences
	if ch == 0x1b {
		ch2, err := e.readByte()
		if err != nil {
			return "escape", nil
		}
		if ch2 == '[' {
			ch3, err := e.readByte()
			if err != nil {
				return "escape", nil
			}
			switch ch3 {
			case 'A':
				return "up", nil
			case 'B':
				return "down", nil
			case 'C':
				return "right", nil
			case 'D':
				return "left", nil
			case 'H':
				return "home", nil
			case 'F':
				return "end", nil
			case 'Z':
				return "shift-tab", nil
			case '3':
				// Delete key: ESC[3~
				e.readByte() // skip ~
				return "delete", nil
			case 'I':
				// Focus gained - ignore
				return e.readKey()
			case 'O':
				// Focus lost - ignore
				return e.readKey()
			}
			// Handle CSI sequences like ESC[200~ (bracketed paste)
			if ch3 >= '0' && ch3 <= '9' {
				// Skip the whole sequence
				debugLog("readKey: skipping CSI sequence starting with %c", ch3)
				e.skipToTerminator()
				return e.readKey()
			}
			// Unknown CSI sequence - skip to terminator
			debugLog("readKey: unknown CSI %c, skipping", ch3)
			if ch3 < 0x40 || ch3 > 0x7E {
				e.skipToTerminator()
			}
			return e.readKey()
		}
		// Unknown escape sequence - treat as escape
		debugLog("readKey: unknown escape sequence starting with 0x%02x", ch2)
		return "escape", nil
	}

	// Control characters
	switch ch {
	case 0x01: // Ctrl-A
		return "home", nil
	case 0x03: // Ctrl-C
		return "ctrl-c", nil
	case 0x04: // Ctrl-D
		return "ctrl-d", nil
	case 0x05: // Ctrl-E
		return "end", nil
	case 0x09: // Tab
		return "tab", nil
	case 0x0d, 0x0a: // Enter
		return "enter", nil
	case 0x7f, 0x08: // Backspace
		return "backspace", nil
	case 0x15: // Ctrl-U
		return "ctrl-u", nil
	case 0x17: // Ctrl-W
		return "ctrl-w", nil
	}

	return string(ch), nil
}

// clearLine clears the current line display.
func (e *LineEditor) clearLine() {
	// Move to start of line, clear to end
	fmt.Print("\r\033[K")
}

// render displays the current line with prompt and any completion popup.
func (e *LineEditor) render(prompt string) {
	// STEP 1: Clear any previously displayed popup lines
	if e.popupLineCount > 0 {
		for i := 0; i < e.popupLineCount; i++ {
			fmt.Print("\n\033[2K")
		}
		fmt.Printf("\033[%dA\r", e.popupLineCount)
		e.popupLineCount = 0
	}

	// STEP 2: Clear and redraw input line
	fmt.Print("\r\033[K")
	fmt.Print(prompt)
	fmt.Print(string(e.line))

	// STEP 3: Draw popup below if active
	if e.showPopup && len(e.completions) > 0 {
		e.renderPopup(prompt)
	}

	// STEP 4: Position cursor on input line
	fmt.Printf("\r\033[%dC", len(prompt)+e.cursor)
}

// typeIndicator returns a single-letter type indicator for completion types.
func typeIndicator(t string) string {
	switch t {
	case "arg-placeholder":
		return "A"
	case "flag":
		return "F"
	case "command":
		return "C"
	case "subcommand":
		return "S"
	case "value":
		return "V"
	default:
		if len(t) > 0 {
			return strings.ToUpper(t[:1])
		}
		return "?"
	}
}

// completionText returns the display text for a completion candidate.
func completionText(c CompletionCandidate) string {
	if c.Type == "arg-placeholder" && c.Name != "" {
		return fmt.Sprintf("<%s>", c.Name)
	}
	return c.Text
}

// renderPopup displays the completion popup below the current line.
func (e *LineEditor) renderPopup(prompt string) {
	maxDisplay := min(len(e.completions), 10)
	termWidth := e.getTerminalWidth()

	// Leave safety margin to prevent wrapping
	maxLen := termWidth - 2
	if maxLen < 40 {
		maxLen = 40
	}

	e.popupLineCount = maxDisplay

	// Calculate the width needed for the name column based on longest name
	nameWidth := 0
	for i := 0; i < maxDisplay; i++ {
		text := completionText(e.completions[i])
		if len(text) > nameWidth {
			nameWidth = len(text)
		}
	}
	// Add 2 for surrounding spaces, cap at reasonable max
	nameWidth += 2
	if nameWidth > 30 {
		nameWidth = 30
	}
	if nameWidth < 8 {
		nameWidth = 8
	}

	for i := 0; i < maxDisplay; i++ {
		c := e.completions[i]

		// Move to next line, go to column 1, and clear the line
		fmt.Print("\n\r\033[K")

		// Build the display line
		prefix := "  "
		if i == e.selected {
			prefix = "> "
		}

		text := completionText(c)
		if len(text) > nameWidth-2 {
			text = text[:nameWidth-5] + "..."
		}

		// Format: "> name     [T] help..."
		// Use dynamic width for name column
		formatStr := fmt.Sprintf("%%s%%-%ds [%%s]", nameWidth)
		line := fmt.Sprintf(formatStr, prefix, text, typeIndicator(c.Type))

		// Add help text if there's room
		if c.Help != "" {
			remaining := maxLen - len(line) - 1
			if remaining > 10 {
				help := c.Help
				if len(help) > remaining {
					help = help[:remaining-3] + "..."
				}
				line += " " + help
			}
		}

		// Final truncation safety
		if len(line) > maxLen {
			line = line[:maxLen]
		}

		// Color: inverse for selected, dim for others
		if i == e.selected {
			fmt.Printf("\033[7m%s\033[0m", line)
		} else {
			fmt.Printf("\033[2m%s\033[0m", line)
		}
	}

	// Move cursor back up to the input line
	if maxDisplay > 0 {
		fmt.Printf("\033[%dA\r", maxDisplay)
	}
}

// clearPopup removes the popup display.
func (e *LineEditor) clearPopup() {
	if e.popupLineCount == 0 {
		return
	}

	// Move down to popup area and clear each line
	for i := 0; i < e.popupLineCount; i++ {
		fmt.Print("\n\033[2K")
	}
	// Move back up
	fmt.Printf("\033[%dA", e.popupLineCount)
	fmt.Print("\r")
	e.popupLineCount = 0
}

// getCompletions fetches completions from the interpreter.
func (e *LineEditor) getCompletions() {
	// Build the full script including any accumulated multi-line input
	script := e.inputBuffer
	if script != "" {
		script += "\n"
	}
	script += string(e.line)
	pos := len(script)

	debugLog("getCompletions: script=%q pos=%d", script, pos)

	// Use Call API for proper argument handling (handles unbalanced braces, etc.)
	result, err := e.interp.Call("usage", "complete", script, pos)
	if err != nil {
		debugLog("getCompletions: error=%v", err)
		e.completions = nil
		return
	}
	debugLog("getCompletions: result=%q", result.String())

	// Parse the result
	e.completions = nil
	list, err := result.List()
	if err != nil {
		return
	}

	for _, item := range list {
		dict, err := item.Dict()
		if err != nil {
			continue
		}

		c := CompletionCandidate{}
		if v, ok := dict.Items["text"]; ok {
			c.Text = v.String()
		}
		if v, ok := dict.Items["type"]; ok {
			c.Type = v.String()
		}
		if v, ok := dict.Items["help"]; ok {
			c.Help = v.String()
		}
		if v, ok := dict.Items["name"]; ok {
			c.Name = v.String()
		}

		e.completions = append(e.completions, c)
	}
}

// applyCompletion inserts the selected completion into the line.
func (e *LineEditor) applyCompletion() {
	if len(e.completions) == 0 || e.selected < 0 || e.selected >= len(e.completions) {
		return
	}

	c := e.completions[e.selected]
	if c.Type == "arg-placeholder" {
		// Don't insert placeholders, just close popup
		e.showPopup = false
		e.completions = nil
		return
	}

	// Find the start of the word being completed
	wordStart := e.cursor
	for wordStart > 0 && !isWordBreak(e.line[wordStart-1]) {
		wordStart--
	}

	// Replace the current word with the completion
	newLine := make([]rune, 0, len(e.line)+len(c.Text))
	newLine = append(newLine, e.line[:wordStart]...)
	newLine = append(newLine, []rune(c.Text)...)
	newLine = append(newLine, ' ') // Add space after completion
	newLine = append(newLine, e.line[e.cursor:]...)

	e.line = newLine
	e.cursor = wordStart + len(c.Text) + 1

	e.showPopup = false
	e.completions = nil
}

func isWordBreak(r rune) bool {
	return r == ' ' || r == '\t' || r == ';' || r == '\n' || r == '{' || r == '}'
}

// startKeyReader starts the persistent key reader goroutine if not already running
func (e *LineEditor) startKeyReader() {
	if e.readerRunning {
		return
	}
	e.keyChan = make(chan keyResult, 16) // Larger buffer to prevent blocking
	e.readerRunning = true
	go func() {
		for {
			key, err := e.readKey()
			debugLog("readKey returned: %q err=%v", key, err)
			e.keyChan <- keyResult{key, err}
			if err != nil {
				e.readerRunning = false
				return
			}
		}
	}()
}

// ReadLine reads a complete line of input with completion support.
func (e *LineEditor) ReadLine(prompt string) (string, error) {
	if err := e.enterRawMode(); err != nil {
		return "", err
	}
	defer e.exitRawMode()

	// Set up SIGWINCH handler for terminal resize
	sigwinch := make(chan os.Signal, 1)
	signal.Notify(sigwinch, syscall.SIGWINCH)
	defer signal.Stop(sigwinch)

	// Start the persistent key reader if not already running
	e.startKeyReader()

	e.line = nil
	e.cursor = 0
	e.showPopup = false
	e.completions = nil
	e.selected = 0

	e.render(prompt)

	for {
		// Wait for either a key press or a resize signal
		var key string
		var err error

		select {
		case <-sigwinch:
			// Terminal resized - just re-render
			e.render(prompt)
			continue
		case kr := <-e.keyChan:
			key = kr.key
			err = kr.err
		}

		if err != nil {
			if err == io.EOF {
				return "", io.EOF
			}
			return "", err
		}

		debugLog("processing key: %q", key)

		switch key {
		case "enter":
			if e.showPopup && len(e.completions) > 0 {
				// Apply selected completion
				e.applyCompletion()
				e.render(prompt)
			} else {
				e.clearPopup()
				fmt.Print("\r\n")
				return string(e.line), nil
			}

		case "ctrl-c":
			e.clearPopup()
			fmt.Print("\r\n")
			return "", fmt.Errorf("interrupted")

		case "ctrl-d":
			if len(e.line) == 0 {
				e.clearPopup()
				fmt.Print("\r\n")
				return "", io.EOF
			}
			// Delete char at cursor
			if e.cursor < len(e.line) {
				e.line = append(e.line[:e.cursor], e.line[e.cursor+1:]...)
				e.hidePopup()
			}

		case "tab":
			if e.showPopup && len(e.completions) > 0 {
				// Cycle forward through completions
				e.selected = (e.selected + 1) % len(e.completions)
			} else {
				// Get completions
				e.getCompletions()
				e.selected = 0
				e.showPopup = len(e.completions) > 0
			}

		case "shift-tab":
			if e.showPopup && len(e.completions) > 0 {
				// Cycle backward through completions
				e.selected--
				if e.selected < 0 {
					e.selected = len(e.completions) - 1
				}
			} else {
				// Get completions and select last item
				e.getCompletions()
				if len(e.completions) > 0 {
					e.selected = len(e.completions) - 1
					e.showPopup = true
				}
			}

		case "up":
			if e.showPopup && len(e.completions) > 0 {
				e.selected--
				if e.selected < 0 {
					e.selected = len(e.completions) - 1
				}
			}

		case "down":
			if e.showPopup && len(e.completions) > 0 {
				e.selected = (e.selected + 1) % len(e.completions)
			}

		case "left":
			if e.cursor > 0 {
				e.cursor--
			}
			e.hidePopup()

		case "right":
			if e.cursor < len(e.line) {
				e.cursor++
			}
			e.hidePopup()

		case "home":
			e.cursor = 0
			e.hidePopup()

		case "end":
			e.cursor = len(e.line)
			e.hidePopup()

		case "backspace":
			if e.cursor > 0 {
				e.line = append(e.line[:e.cursor-1], e.line[e.cursor:]...)
				e.cursor--
				e.hidePopup()
			}

		case "delete":
			if e.cursor < len(e.line) {
				e.line = append(e.line[:e.cursor], e.line[e.cursor+1:]...)
				e.hidePopup()
			}

		case "ctrl-u":
			// Clear line before cursor
			e.line = e.line[e.cursor:]
			e.cursor = 0
			e.hidePopup()

		case "ctrl-w":
			// Delete word before cursor
			newCursor := e.cursor
			// Skip trailing spaces
			for newCursor > 0 && e.line[newCursor-1] == ' ' {
				newCursor--
			}
			// Skip word
			for newCursor > 0 && e.line[newCursor-1] != ' ' {
				newCursor--
			}
			e.line = append(e.line[:newCursor], e.line[e.cursor:]...)
			e.cursor = newCursor
			e.hidePopup()

		case "escape":
			if e.showPopup {
				e.hidePopup()
			}

		default:
			// Insert character
			if len(key) == 1 {
				ch := rune(key[0])
				if ch >= 32 && ch < 127 {
					newLine := make([]rune, len(e.line)+1)
					copy(newLine, e.line[:e.cursor])
					newLine[e.cursor] = ch
					copy(newLine[e.cursor+1:], e.line[e.cursor:])
					e.line = newLine
					e.cursor++
					e.hidePopup()
				}
			}
		}

		e.render(prompt)
	}
}

// hidePopup clears and hides the completion popup.
func (e *LineEditor) hidePopup() {
	if e.showPopup || e.popupLineCount > 0 {
		e.clearPopup()
		e.showPopup = false
		e.completions = nil
	}
}

// SetInputBuffer sets the accumulated multi-line input for context.
func (e *LineEditor) SetInputBuffer(buf string) {
	e.inputBuffer = buf
}

func min(a, b int) int {
	if a < b {
		return a
	}
	return b
}

// runREPLWithEditor runs an interactive REPL with the line editor.
func runREPLWithEditor(i *feather.Interp) {
	editor := NewLineEditor(i)
	var inputBuffer string

	fmt.Println("Feather REPL - Press Tab for completions, Ctrl-D to exit")

	for {
		prompt := "% "
		if inputBuffer != "" {
			prompt = "> "
		}

		editor.SetInputBuffer(inputBuffer)
		line, err := editor.ReadLine(prompt)
		if err != nil {
			if err == io.EOF {
				if inputBuffer != "" {
					fmt.Println()
					fmt.Println("Incomplete input, discarded")
				}
				break
			}
			if strings.Contains(err.Error(), "interrupted") {
				inputBuffer = ""
				continue
			}
			break
		}

		if inputBuffer != "" {
			inputBuffer += "\n" + line
		} else {
			inputBuffer = line
		}

		parseResult := i.Parse(inputBuffer)
		if parseResult.Status == feather.ParseIncomplete {
			continue
		}

		if parseResult.Status == feather.ParseError {
			fmt.Fprintf(os.Stderr, "error: %s\n", parseResult.Message)
			inputBuffer = ""
			continue
		}

		result, err := i.Eval(inputBuffer)
		if err != nil {
			fmt.Fprintf(os.Stderr, "error: %s\n", err.Error())
		} else if result.String() != "" {
			fmt.Println(result.String())
		}
		inputBuffer = ""
	}
}
