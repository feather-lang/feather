// feather-httpd is an example HTTP server configurable via the feather TCL interpreter.
//
// Usage:
//
//	feather-httpd [script.tcl]
//
// If a script is provided, it is evaluated at startup. Then, a REPL is started
// for interactive configuration. The server can be controlled via TCL commands:
//
//	route GET /path {script}   - register a route handler
//	listen 8080                - start the HTTP server on a port
//	stop                       - stop the HTTP server
//	response body              - set response body (in handler context)
//	status code                - set HTTP status code (in handler context)
//	header name value          - set response header (in handler context)
//	request method             - get request method (in handler context)
//	request path               - get request path (in handler context)
//	request header name        - get request header (in handler context)
//	request query name         - get query parameter (in handler context)
//	template list              - list available templates
//	template show name         - show template source
//	template render name data  - render template with data to response
//	template errors            - get dict of templates with parse errors
//
// Templates are loaded from the "templates" directory and automatically
// reloaded when files change. Supported extensions: .html, .tmpl
//
// Example session:
//
//	% route GET / {response "Hello, World!"}
//	% route GET /time {response [clock format [clock seconds]]}
//	% listen 8080
//	Listening on :8080
//	% stop
//	Server stopped
package main

import (
	"bufio"
	"context"
	"fmt"
	"html/template"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"sync"

	"github.com/feather-lang/feather"
)

// TemplateInfo holds a parsed template and its file modification time.
type TemplateInfo struct {
	Template *template.Template
	ModTime  int64
	Error    error
}

// HTTPServer wraps an HTTP server with feather integration.
type HTTPServer struct {
	interp      *feather.Interp
	mux         *http.ServeMux
	server      *http.Server
	mu          sync.RWMutex
	routes      map[string]string // "METHOD /path" -> script
	running     bool
	templateDir string
	templates   map[string]*TemplateInfo
	templateMu  sync.RWMutex
}

// RequestContext holds per-request state for handler scripts.
type RequestContext struct {
	Request      *http.Request
	Writer       http.ResponseWriter
	StatusCode   int
	Headers      map[string]string
	BodyWritten  bool
	ResponseBody string
}

// Global request context (thread-local would be better, but this is a demo)
var currentRequest *RequestContext
var requestMu sync.Mutex

func main() {
	i := feather.New()
	defer i.Close()

	srv := &HTTPServer{
		interp:      i,
		mux:         http.NewServeMux(),
		routes:      make(map[string]string),
		templateDir: "templates",
		templates:   make(map[string]*TemplateInfo),
	}

	// Register HTTP commands
	srv.registerCommands()

	// If a script file is provided, evaluate it
	if len(os.Args) > 1 {
		script, err := os.ReadFile(os.Args[1])
		if err != nil {
			fmt.Fprintf(os.Stderr, "error reading script: %v\n", err)
			os.Exit(1)
		}
		if _, err := i.Eval(string(script)); err != nil {
			fmt.Fprintf(os.Stderr, "error: %v\n", err)
			os.Exit(1)
		}
	}

	// Check if stdin is a TTY for REPL
	stat, _ := os.Stdin.Stat()
	if (stat.Mode() & os.ModeCharDevice) != 0 {
		runREPL(i)
	} else {
		// Non-interactive: read and eval stdin, then wait if server is running
		script, err := io.ReadAll(os.Stdin)
		if err != nil {
			fmt.Fprintf(os.Stderr, "error reading stdin: %v\n", err)
			os.Exit(1)
		}
		if len(script) > 0 {
			if _, err := i.Eval(string(script)); err != nil {
				fmt.Fprintf(os.Stderr, "error: %v\n", err)
				os.Exit(1)
			}
		}
		// If server is running, block forever
		srv.mu.RLock()
		running := srv.running
		srv.mu.RUnlock()
		if running {
			select {} // Block forever
		}
	}
}

func (s *HTTPServer) registerCommands() {
	// Register commands directly in the Commands map
	s.interp.Commands["route"] = s.cmdRoute
	s.interp.Commands["listen"] = s.cmdListen
	s.interp.Commands["stop"] = s.cmdStop
	s.interp.Commands["response"] = s.cmdResponse
	s.interp.Commands["status"] = s.cmdStatus
	s.interp.Commands["header"] = s.cmdHeader
	s.interp.Commands["request"] = s.cmdRequest
	s.interp.Commands["template"] = s.cmdTemplate
}

// cmdRoute registers a route handler.
// Usage: route METHOD /path {script}
func (s *HTTPServer) cmdRoute(i *feather.Interp, cmd feather.FeatherObj, args []feather.FeatherObj) feather.FeatherResult {
	if len(args) < 3 {
		i.SetErrorString("wrong # args: should be \"route method path script\"")
		return feather.ResultError
	}

	method := strings.ToUpper(i.GetString(args[0]))
	path := i.GetString(args[1])
	script := i.GetString(args[2])

	key := method + " " + path
	s.mu.Lock()
	s.routes[key] = script
	s.mu.Unlock()

	i.SetResultString("")
	return feather.ResultOK
}

// cmdListen starts the HTTP server.
// Usage: listen port
func (s *HTTPServer) cmdListen(i *feather.Interp, cmd feather.FeatherObj, args []feather.FeatherObj) feather.FeatherResult {
	if len(args) < 1 {
		i.SetErrorString("wrong # args: should be \"listen port\"")
		return feather.ResultError
	}

	port := i.GetString(args[0])
	addr := ":" + port

	s.mu.Lock()
	if s.running {
		s.mu.Unlock()
		i.SetErrorString("server already running")
		return feather.ResultError
	}

	s.server = &http.Server{
		Addr:    addr,
		Handler: s,
	}
	s.running = true
	s.mu.Unlock()

	// Start server in background
	go func() {
		err := s.server.ListenAndServe()
		if err != nil && err != http.ErrServerClosed {
			fmt.Fprintf(os.Stderr, "server error: %v\n", err)
		}
		s.mu.Lock()
		s.running = false
		s.mu.Unlock()
	}()

	fmt.Printf("Listening on %s\n", addr)
	i.SetResultString("")
	return feather.ResultOK
}

// cmdStop stops the HTTP server.
// Usage: stop
func (s *HTTPServer) cmdStop(i *feather.Interp, cmd feather.FeatherObj, args []feather.FeatherObj) feather.FeatherResult {
	s.mu.Lock()
	if !s.running || s.server == nil {
		s.mu.Unlock()
		i.SetErrorString("server not running")
		return feather.ResultError
	}
	server := s.server
	s.mu.Unlock()

	if err := server.Shutdown(context.Background()); err != nil {
		i.SetErrorString(fmt.Sprintf("shutdown error: %v", err))
		return feather.ResultError
	}

	fmt.Println("Server stopped")
	i.SetResultString("")
	return feather.ResultOK
}

// cmdResponse sets the response body.
// Usage: response body
func (s *HTTPServer) cmdResponse(i *feather.Interp, cmd feather.FeatherObj, args []feather.FeatherObj) feather.FeatherResult {
	requestMu.Lock()
	ctx := currentRequest
	requestMu.Unlock()

	if ctx == nil {
		i.SetErrorString("response: not in request context")
		return feather.ResultError
	}

	if len(args) < 1 {
		i.SetErrorString("wrong # args: should be \"response body\"")
		return feather.ResultError
	}

	ctx.ResponseBody = i.GetString(args[0])
	i.SetResultString("")
	return feather.ResultOK
}

// cmdStatus sets the HTTP status code.
// Usage: status code
func (s *HTTPServer) cmdStatus(i *feather.Interp, cmd feather.FeatherObj, args []feather.FeatherObj) feather.FeatherResult {
	requestMu.Lock()
	ctx := currentRequest
	requestMu.Unlock()

	if ctx == nil {
		i.SetErrorString("status: not in request context")
		return feather.ResultError
	}

	if len(args) < 1 {
		i.SetErrorString("wrong # args: should be \"status code\"")
		return feather.ResultError
	}

	code, err := i.GetInt(args[0])
	if err != nil {
		i.SetErrorString(fmt.Sprintf("status: invalid code: %v", err))
		return feather.ResultError
	}

	ctx.StatusCode = int(code)
	i.SetResultString("")
	return feather.ResultOK
}

// cmdHeader sets a response header.
// Usage: header name value
func (s *HTTPServer) cmdHeader(i *feather.Interp, cmd feather.FeatherObj, args []feather.FeatherObj) feather.FeatherResult {
	requestMu.Lock()
	ctx := currentRequest
	requestMu.Unlock()

	if ctx == nil {
		i.SetErrorString("header: not in request context")
		return feather.ResultError
	}

	if len(args) < 2 {
		i.SetErrorString("wrong # args: should be \"header name value\"")
		return feather.ResultError
	}

	name := i.GetString(args[0])
	value := i.GetString(args[1])
	ctx.Headers[name] = value

	i.SetResultString("")
	return feather.ResultOK
}

// cmdRequest gets request information.
// Usage: request method | path | header name | query name | body
func (s *HTTPServer) cmdRequest(i *feather.Interp, cmd feather.FeatherObj, args []feather.FeatherObj) feather.FeatherResult {
	requestMu.Lock()
	ctx := currentRequest
	requestMu.Unlock()

	if ctx == nil {
		i.SetErrorString("request: not in request context")
		return feather.ResultError
	}

	if len(args) < 1 {
		i.SetErrorString("wrong # args: should be \"request subcommand ?arg?\"")
		return feather.ResultError
	}

	subcmd := i.GetString(args[0])
	switch subcmd {
	case "method":
		i.SetResultString(ctx.Request.Method)
	case "path":
		i.SetResultString(ctx.Request.URL.Path)
	case "header":
		if len(args) < 2 {
			i.SetErrorString("wrong # args: should be \"request header name\"")
			return feather.ResultError
		}
		name := i.GetString(args[1])
		i.SetResultString(ctx.Request.Header.Get(name))
	case "query":
		if len(args) < 2 {
			i.SetErrorString("wrong # args: should be \"request query name\"")
			return feather.ResultError
		}
		name := i.GetString(args[1])
		i.SetResultString(ctx.Request.URL.Query().Get(name))
	case "body":
		body, err := io.ReadAll(ctx.Request.Body)
		if err != nil {
			i.SetErrorString(fmt.Sprintf("request body: %v", err))
			return feather.ResultError
		}
		i.SetResultString(string(body))
	default:
		i.SetErrorString(fmt.Sprintf("request: unknown subcommand %q", subcmd))
		return feather.ResultError
	}

	return feather.ResultOK
}

// refreshTemplates scans the template directory and reloads changed templates.
func (s *HTTPServer) refreshTemplates() {
	s.templateMu.Lock()
	defer s.templateMu.Unlock()

	// Track which templates still exist
	seen := make(map[string]bool)

	// Walk the template directory
	filepath.Walk(s.templateDir, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return nil
		}
		if info.IsDir() {
			return nil
		}
		// Only process .html and .tmpl files
		ext := filepath.Ext(path)
		if ext != ".html" && ext != ".tmpl" {
			return nil
		}

		// Use relative path as template name
		relPath, err := filepath.Rel(s.templateDir, path)
		if err != nil {
			return nil
		}
		name := relPath

		seen[name] = true
		modTime := info.ModTime().UnixNano()

		// Check if we need to reload
		existing, ok := s.templates[name]
		if ok && existing.ModTime == modTime {
			return nil
		}

		// Parse the template
		tmpl, parseErr := template.ParseFiles(path)
		s.templates[name] = &TemplateInfo{
			Template: tmpl,
			ModTime:  modTime,
			Error:    parseErr,
		}
		return nil
	})

	// Remove templates that no longer exist
	for name := range s.templates {
		if !seen[name] {
			delete(s.templates, name)
		}
	}
}

// cmdTemplate handles template subcommands.
// Usage: template list | template render name data | template errors
func (s *HTTPServer) cmdTemplate(i *feather.Interp, cmd feather.FeatherObj, args []feather.FeatherObj) feather.FeatherResult {
	if len(args) < 1 {
		i.SetErrorString("wrong # args: should be \"template subcommand ?args?\"")
		return feather.ResultError
	}

	subcmd := i.GetString(args[0])
	switch subcmd {
	case "list":
		return s.cmdTemplateList(i)
	case "render":
		return s.cmdTemplateRender(i, args[1:])
	case "errors":
		return s.cmdTemplateErrors(i)
	case "show":
		return s.cmdTemplateShow(i, args[1:])
	default:
		i.SetErrorString(fmt.Sprintf("template: unknown subcommand %q", subcmd))
		return feather.ResultError
	}
}

// cmdTemplateList lists available templates.
func (s *HTTPServer) cmdTemplateList(i *feather.Interp) feather.FeatherResult {
	s.refreshTemplates()

	s.templateMu.RLock()
	defer s.templateMu.RUnlock()

	list := i.NewListObj()
	for name := range s.templates {
		i.ListAppendObj(list, i.InternString(name))
	}

	i.SetResult(list)
	return feather.ResultOK
}

// cmdTemplateRender renders a template with data to the response.
func (s *HTTPServer) cmdTemplateRender(i *feather.Interp, args []feather.FeatherObj) feather.FeatherResult {
	requestMu.Lock()
	ctx := currentRequest
	requestMu.Unlock()

	if ctx == nil {
		i.SetErrorString("template render: not in request context")
		return feather.ResultError
	}

	if len(args) < 2 {
		i.SetErrorString("wrong # args: should be \"template render name data\"")
		return feather.ResultError
	}

	name := i.GetString(args[0])
	dataObj := args[1]

	s.refreshTemplates()

	s.templateMu.RLock()
	info, ok := s.templates[name]
	s.templateMu.RUnlock()

	if !ok {
		i.SetErrorString(fmt.Sprintf("template render: template %q not found", name))
		return feather.ResultError
	}

	if info.Error != nil {
		i.SetErrorString(fmt.Sprintf("template render: template %q has parse error: %v", name, info.Error))
		return feather.ResultError
	}

	// Convert TCL data to Go map
	data := s.tclToGoData(i, dataObj)

	// Render template to buffer
	var buf strings.Builder
	if err := info.Template.Execute(&buf, data); err != nil {
		i.SetErrorString(fmt.Sprintf("template render: %v", err))
		return feather.ResultError
	}

	ctx.ResponseBody = buf.String()
	i.SetResultString("")
	return feather.ResultOK
}

// cmdTemplateShow returns the source of a template.
func (s *HTTPServer) cmdTemplateShow(i *feather.Interp, args []feather.FeatherObj) feather.FeatherResult {
	if len(args) < 1 {
		i.SetErrorString("wrong # args: should be \"template show name\"")
		return feather.ResultError
	}

	name := i.GetString(args[0])
	path := filepath.Join(s.templateDir, name)

	content, err := os.ReadFile(path)
	if err != nil {
		i.SetErrorString(fmt.Sprintf("template show: %v", err))
		return feather.ResultError
	}

	i.SetResultString(string(content))
	return feather.ResultOK
}

// cmdTemplateErrors returns a dict of templates and their parsing errors.
func (s *HTTPServer) cmdTemplateErrors(i *feather.Interp) feather.FeatherResult {
	s.refreshTemplates()

	s.templateMu.RLock()
	defer s.templateMu.RUnlock()

	dict := i.NewDictObj()
	for name, info := range s.templates {
		if info.Error != nil {
			i.DictSetObj(dict, name, i.InternString(info.Error.Error()))
		}
	}
	i.SetResult(dict)
	return feather.ResultOK
}

// tclToGoData converts a TCL object to Go data suitable for template execution.
func (s *HTTPServer) tclToGoData(i *feather.Interp, obj feather.FeatherObj) any {
	// Check native dict first (avoids infinite recursion from shimmering)
	if i.IsNativeDict(obj) {
		dictItems, dictOrder, err := i.GetDict(obj)
		if err == nil {
			result := make(map[string]any)
			for _, key := range dictOrder {
				val := dictItems[key]
				result[key] = s.tclToGoData(i, val)
			}
			return result
		}
	}

	// Check native list (avoids infinite recursion from shimmering)
	if i.IsNativeList(obj) {
		listItems, err := i.GetList(obj)
		if err == nil {
			result := make([]any, len(listItems))
			for idx, elem := range listItems {
				result[idx] = s.tclToGoData(i, elem)
			}
			return result
		}
	}

	// Default to string
	return i.GetString(obj)
}

// tclList formats strings as a proper TCL list.
func tclList(items []string) string {
	var parts []string
	for _, item := range items {
		parts = append(parts, tclQuote(item))
	}
	return strings.Join(parts, " ")
}

// tclQuote quotes a string for use in a TCL list if necessary.
func tclQuote(s string) string {
	if s == "" {
		return "{}"
	}
	needsQuote := false
	for _, c := range s {
		if c == ' ' || c == '\t' || c == '\n' || c == '{' || c == '}' || c == '"' || c == '\\' {
			needsQuote = true
			break
		}
	}
	if !needsQuote {
		return s
	}
	// Use braces for quoting
	return "{" + s + "}"
}

// ServeHTTP implements http.Handler.
func (s *HTTPServer) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	// Find matching route
	key := r.Method + " " + r.URL.Path
	s.mu.RLock()
	script, ok := s.routes[key]
	s.mu.RUnlock()

	if !ok {
		// Try without method (ANY)
		key = "ANY " + r.URL.Path
		s.mu.RLock()
		script, ok = s.routes[key]
		s.mu.RUnlock()
	}

	if !ok {
		http.NotFound(w, r)
		return
	}

	// Set up request context
	ctx := &RequestContext{
		Request:    r,
		Writer:     w,
		StatusCode: 200,
		Headers:    make(map[string]string),
	}

	requestMu.Lock()
	currentRequest = ctx
	requestMu.Unlock()

	defer func() {
		requestMu.Lock()
		currentRequest = nil
		requestMu.Unlock()
	}()

	// Execute the handler script
	_, err := s.interp.Eval(script)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}

	// Write response
	for name, value := range ctx.Headers {
		w.Header().Set(name, value)
	}
	w.WriteHeader(ctx.StatusCode)
	if ctx.ResponseBody != "" {
		w.Write([]byte(ctx.ResponseBody))
	}
}

func runREPL(i *feather.Interp) {
	scanner := bufio.NewScanner(os.Stdin)
	var inputBuffer string

	for {
		if inputBuffer == "" {
			fmt.Print("% ")
		} else {
			fmt.Print("> ")
		}

		if !scanner.Scan() {
			break
		}

		line := scanner.Text()
		if inputBuffer != "" {
			inputBuffer += "\n" + line
		} else {
			inputBuffer = line
		}

		// Check if input is complete
		parseResult := i.Parse(inputBuffer)
		if parseResult.Status == feather.ParseIncomplete {
			continue
		}

		if parseResult.Status == feather.ParseError {
			fmt.Fprintf(os.Stderr, "error: %s\n", parseResult.Message)
			inputBuffer = ""
			continue
		}

		// Evaluate the complete input
		result, err := i.Eval(inputBuffer)
		if err != nil {
			fmt.Fprintf(os.Stderr, "error: %s\n", err.Error())
		} else if result.String() != "" {
			fmt.Println(result.String())
		}
		inputBuffer = ""
	}

	if err := scanner.Err(); err != nil {
		fmt.Fprintf(os.Stderr, "error reading input: %v\n", err)
	}
}
