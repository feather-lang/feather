# tailcall Builtin Implementation

## Summary of Our Implementation

The `tailcall` command in feather replaces the currently executing procedure with another command, enabling tail-call optimization for recursive procedures.

Our implementation in `src/builtin_tailcall.c`:

1. Validates that at least one argument (the command name) is provided
2. Checks that we are inside a proc (frame level > 0)
3. Pops the current frame to make the caller's frame active
4. Executes the specified command in the caller's context
5. Returns TCL_RETURN with appropriate return options (-code 0 -level 1) on success

## TCL Features We Support

- **Basic syntax**: `tailcall command ?arg ...?`
- **Procedure replacement**: The current procedure is replaced with the specified command
- **Tail recursion**: Enables efficient recursive procedures without growing the call stack
- **Error propagation**: Errors from the tailcalled command are properly propagated
- **Proc-only restriction**: Correctly rejects tailcall when called at the global level (level 0)

## TCL Features We Do NOT Support

1. **Namespace context resolution**: According to TCL documentation, the command "will be looked up in the current namespace context, not in the caller's." Our implementation uses `feather_command_exec` with `TCL_EVAL_LOCAL` but may not correctly handle namespace resolution differences.

2. **Lambda/method support**: TCL states tailcall works with "procedure, lambda application, or method." While our implementation checks `level > 0`, we may not have full support for lambda applications or TclOO methods.

3. **Uplevel restriction**: TCL specifies that "This command may not be invoked from within an uplevel into a procedure or inside a catch inside a procedure or lambda." Our implementation does not enforce this restriction - it only checks that level > 0, not whether we are inside an uplevel or catch context.

## Notes on Implementation Differences

1. **Frame handling**: Our implementation manually pops the current frame before executing the tailcalled command. The comment notes that `feather_invoke_proc` will try to pop the frame again, but the second pop will be a no-op. This is an implementation detail that may differ from standard TCL's internal handling.

2. **Return options**: We set return options with `-code 0 -level 1` to signal proper handling by the proc invocation machinery. This mimics TCL's internal signaling mechanism.

3. **Error message wording**: Our error messages are slightly different from standard TCL:
   - We use: "tailcall can only be called from a proc or lambda"
   - TCL may have different wording for this error

4. **Semantic equivalence**: TCL documentation states tailcall is equivalent to `return [uplevel 1 [list command ?arg ...?]]` apart from namespace resolution. Our implementation achieves similar semantics through direct frame manipulation rather than using uplevel.
