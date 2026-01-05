#include "feather.h"
#include "internal.h"
#include "namespace_util.h"
#include "charclass.h"
#include "error_trace.h"

FeatherResult feather_builtin_proc(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj cmd, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);

  // proc requires exactly 3 arguments: name args body
  if (argc != 3) {
    FeatherObj msg = ops->string.intern(
        interp, "wrong # args: should be \"proc name args body\"", 45);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Extract name, params, body
  FeatherObj name = ops->list.shift(interp, args);
  FeatherObj params = ops->list.shift(interp, args);
  FeatherObj body = ops->list.shift(interp, args);

  // Validate parameter specs - each must be 1 or 2 elements
  size_t paramc = ops->list.length(interp, params);
  FeatherObj paramsCopy = ops->list.from(interp, params);
  for (size_t i = 0; i < paramc; i++) {
    FeatherObj paramSpec = ops->list.shift(interp, paramsCopy);
    size_t specLen = ops->list.length(interp, paramSpec);
    if (specLen > 2) {
      // Error: too many fields in argument specifier
      FeatherObj msg = ops->string.intern(interp, "too many fields in argument specifier \"", 39);
      msg = ops->string.concat(interp, msg, paramSpec);
      msg = ops->string.concat(interp, msg, ops->string.intern(interp, "\"", 1));
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
  }

  // Determine the fully qualified proc name
  FeatherObj qualifiedName;
  if (feather_obj_is_qualified(ops, interp, name)) {
    // Name is already qualified (starts with :: or contains ::)
    // Use it as-is, but ensure the namespace exists
    // For "::foo::bar::baz", create namespaces ::foo and ::foo::bar

    // Find the last :: to split namespace from proc name
    long lastSep = feather_obj_find_last_colons(ops, interp, name);

    // Create namespace path if needed (everything before last ::)
    if (lastSep > 0) {
      FeatherObj nsPath = ops->string.slice(interp, name, 0, (size_t)lastSep);
      ops->ns.create(interp, nsPath);
    }

    qualifiedName = name;
  } else {
    // Unqualified name - prepend current namespace
    FeatherObj currentNs = ops->ns.current(interp);

    // Always store with full namespace path
    // Global namespace (::) -> "::name"
    // Other namespace -> "::ns::name"
    if (feather_obj_is_global_ns(ops, interp, currentNs)) {
      // Global namespace: prepend "::"
      qualifiedName = ops->string.intern(interp, "::", 2);
      qualifiedName = ops->string.concat(interp, qualifiedName, name);
    } else {
      // Other namespace: "::ns::name"
      qualifiedName = ops->string.concat(interp, currentNs,
                                         ops->string.intern(interp, "::", 2));
      qualifiedName = ops->string.concat(interp, qualifiedName, name);
    }
  }

  // Register the procedure with its fully qualified name
  feather_register_command(ops, interp, qualifiedName, TCL_CMD_PROC, NULL, params, body);

  // proc returns empty string
  FeatherObj empty = ops->string.intern(interp, "", 0);
  ops->interp.set_result(interp, empty);
  return TCL_OK;
}


// Helper to get parameter name from a param spec (handles {name} or {name default})
static FeatherObj get_param_name(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj paramSpec) {
  size_t specLen = ops->list.length(interp, paramSpec);
  if (specLen >= 1) {
    return ops->list.at(interp, paramSpec, 0);
  }
  return paramSpec;  // Already just a name
}

// Helper to check if a param spec has a default value
static int has_default(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj paramSpec) {
  return ops->list.length(interp, paramSpec) == 2;
}

// Helper to get the default value from a param spec
static FeatherObj get_default(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj paramSpec) {
  return ops->list.at(interp, paramSpec, 1);
}

FeatherResult feather_invoke_proc(const FeatherHostOps *ops, FeatherInterp interp,
                          FeatherObj name, FeatherObj args) {
  // Get the procedure's parameter list and body
  FeatherObj params = 0;
  FeatherObj body = 0;
  FeatherCommandType cmdType = feather_lookup_command(ops, interp, name, NULL, &params, &body);
  if (cmdType != TCL_CMD_PROC || params == 0 || body == 0) {
    return TCL_ERROR;
  }

  // Count arguments and parameters
  size_t argc = ops->list.length(interp, args);
  size_t paramc = ops->list.length(interp, params);

  // Check if this is a variadic proc (last param name is "args")
  int is_variadic = 0;
  size_t bindable_params = paramc;  // Number of params that get individual bindings
  if (paramc > 0) {
    FeatherObj lastParamSpec = ops->list.at(interp, params, paramc - 1);
    FeatherObj lastParamName = get_param_name(ops, interp, lastParamSpec);
    if (lastParamName != 0) {
      is_variadic = feather_obj_is_args_param(ops, interp, lastParamName);
      if (is_variadic) {
        bindable_params = paramc - 1;  // args collects the rest
      }
    }
  }

  // Calculate minimum required arguments
  // Find the rightmost parameter without a default (before args) - everything up to it is required
  size_t min_args = 0;
  for (size_t i = 0; i < bindable_params; i++) {
    FeatherObj paramSpec = ops->list.at(interp, params, i);
    if (!has_default(ops, interp, paramSpec)) {
      // This param is required, so all params up to and including it are needed
      min_args = i + 1;
    }
  }

  // Calculate maximum arguments
  size_t max_args = is_variadic ? (size_t)-1 : bindable_params;

  // Check argument count
  int args_ok = (argc >= min_args) && (argc <= max_args);

  if (!args_ok) {
    // Build error message: wrong # args: should be "name param1 ?param2? ..."
    FeatherObj displayName = feather_get_display_name(ops, interp, name);

    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"", 25);
    msg = ops->string.concat(interp, msg, displayName);

    // Add parameters to error message
    for (size_t i = 0; i < paramc; i++) {
      FeatherObj space = ops->string.intern(interp, " ", 1);
      msg = ops->string.concat(interp, msg, space);
      FeatherObj paramSpec = ops->list.at(interp, params, i);
      FeatherObj paramName = get_param_name(ops, interp, paramSpec);

      // For variadic, show "?arg ...?" instead of "args"
      if (is_variadic && i == paramc - 1) {
        FeatherObj argsHint = ops->string.intern(interp, "?arg ...?", 9);
        msg = ops->string.concat(interp, msg, argsHint);
      } else {
        // Show ?param? for any param with a default value
        // (TCL shows them as optional in error messages even when effectively required)
        int has_def = has_default(ops, interp, paramSpec);
        if (has_def) {
          FeatherObj question = ops->string.intern(interp, "?", 1);
          msg = ops->string.concat(interp, msg, question);
          msg = ops->string.concat(interp, msg, paramName);
          msg = ops->string.concat(interp, msg, question);
        } else {
          msg = ops->string.concat(interp, msg, paramName);
        }
      }
    }

    FeatherObj end = ops->string.intern(interp, "\"", 1);
    msg = ops->string.concat(interp, msg, end);

    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Push a new call frame
  // First, get the line number from the parent frame (set by eval before calling us)
  size_t parentLevel = ops->frame.level(interp);
  size_t parentLine = ops->frame.get_line(interp, parentLevel);

  if (ops->frame.push(interp, name, args) != TCL_OK) {
    return TCL_ERROR;
  }

  // Copy the line number from the parent frame to the new frame
  ops->frame.set_line(interp, parentLine);

  // Set the namespace for this frame based on the proc's qualified name
  // For "::counter::incr", the namespace is "::counter"
  // For "incr", the namespace is "::" (global)
  if (feather_obj_is_qualified(ops, interp, name)) {
    // Find the last :: to extract the namespace part
    long lastSep = feather_obj_find_last_colons(ops, interp, name);
    if (lastSep > 0) {
      // Namespace is everything before the last ::
      FeatherObj ns = ops->string.slice(interp, name, 0, (size_t)lastSep);
      ops->frame.set_namespace(interp, ns);
    } else if (lastSep == 0) {
      // Starts with :: but has no more separators, e.g., "::incr"
      // Namespace is "::" (global)
      FeatherObj globalNs = ops->string.intern(interp, "::", 2);
      ops->frame.set_namespace(interp, globalNs);
    }
  }
  // For unqualified names, leave namespace as default (global)

  // Create copy of args for binding (since shift mutates)
  FeatherObj argsList = ops->list.from(interp, args);

  // Bind arguments to parameters
  // Number of args to bind directly (not to args collector)
  size_t args_to_bind = argc;
  if (is_variadic && argc > bindable_params) {
    args_to_bind = bindable_params;
  }

  // Bind the regular parameters
  for (size_t i = 0; i < bindable_params; i++) {
    FeatherObj paramSpec = ops->list.at(interp, params, i);
    FeatherObj paramName = get_param_name(ops, interp, paramSpec);

    if (i < args_to_bind) {
      // Argument provided
      FeatherObj arg = ops->list.shift(interp, argsList);
      ops->var.set(interp, paramName, arg);
    } else {
      // Use default value
      FeatherObj defaultVal = get_default(ops, interp, paramSpec);
      ops->var.set(interp, paramName, defaultVal);
    }
  }

  // Handle args if variadic
  if (is_variadic) {
    FeatherObj argsParamSpec = ops->list.at(interp, params, paramc - 1);
    FeatherObj argsParamName = get_param_name(ops, interp, argsParamSpec);

    // Collect remaining arguments into a list
    FeatherObj collectedArgs = ops->list.create(interp);
    size_t remaining = argc > bindable_params ? argc - bindable_params : 0;
    for (size_t i = 0; i < remaining; i++) {
      FeatherObj arg = ops->list.shift(interp, argsList);
      collectedArgs = ops->list.push(interp, collectedArgs, arg);
    }

    // Bind the list to "args"
    ops->var.set(interp, argsParamName, collectedArgs);
  }

  // Check if this proc has step traces or if we're in a stepped context
  FeatherObj stepTarget = feather_get_step_target();
  int own_step_traces = feather_has_step_traces(ops, interp, name);

  // Evaluate the body as a script
  FeatherResult result;
  if (own_step_traces) {
    // This proc has its own step traces - set it as the step target
    FeatherObj savedTarget = stepTarget;
    feather_set_step_target(name);
    result = feather_script_eval_obj_stepped(ops, interp, body, name, TCL_EVAL_LOCAL);
    feather_set_step_target(savedTarget);
  } else if (stepTarget != 0) {
    // We're in a stepped context from a parent call - continue stepping
    result = feather_script_eval_obj_stepped(ops, interp, body, stepTarget, TCL_EVAL_LOCAL);
  } else {
    // No step tracing needed
    result = feather_script_eval_obj(ops, interp, body, TCL_EVAL_LOCAL);
  }

  // Append stack frame if error in progress
  if (result == TCL_ERROR && feather_error_is_active(ops, interp)) {
    size_t errorLine = ops->frame.get_line(interp, ops->frame.level(interp));
    feather_error_append_frame(ops, interp, name, args, errorLine);
  }

  // Pop the call frame
  ops->frame.pop(interp);

  // Handle TCL_RETURN specially
  if (result == TCL_RETURN) {
    // Get the return options
    FeatherObj opts = ops->interp.get_return_options(interp, result);

    // Parse -code and -level from the options list
    // Options list format: {-code X -level Y}
    int code = TCL_OK;
    int level = 1;

    size_t optsLen = ops->list.length(interp, opts);
    FeatherObj optsCopy = ops->list.from(interp, opts);

    for (size_t i = 0; i + 1 < optsLen; i += 2) {
      FeatherObj key = ops->list.shift(interp, optsCopy);
      FeatherObj val = ops->list.shift(interp, optsCopy);

      if (feather_obj_eq_literal(ops, interp, key, "-code")) {
        int64_t intVal;
        if (ops->integer.get(interp, val, &intVal) == TCL_OK) {
          code = (int)intVal;
        }
      } else if (feather_obj_eq_literal(ops, interp, key, "-level")) {
        int64_t intVal;
        if (ops->integer.get(interp, val, &intVal) == TCL_OK) {
          level = (int)intVal;
        }
      }
    }

    // Decrement level
    level--;

    if (level <= 0) {
      // Level reached 0, apply the -code
      return (FeatherResult)code;
    } else {
      // Level > 0, update options and keep returning TCL_RETURN
      FeatherObj newOpts = ops->list.create(interp);
      newOpts = ops->list.push(interp, newOpts,
                               ops->string.intern(interp, "-code", 5));
      newOpts = ops->list.push(interp, newOpts,
                               ops->integer.create(interp, code));
      newOpts = ops->list.push(interp, newOpts,
                               ops->string.intern(interp, "-level", 6));
      newOpts = ops->list.push(interp, newOpts,
                               ops->integer.create(interp, level));
      ops->interp.set_return_options(interp, newOpts);
      return TCL_RETURN;
    }
  }

  return result;
}

void feather_register_proc_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  FeatherObj e = feather_usage_about(ops, interp,
    "Create a TCL procedure",
    "Creates a new procedure named name, replacing any existing command or "
    "procedure there may have been by that name. Whenever the new command is "
    "invoked, the contents of body will be executed by the interpreter.\n\n"
    "Normally, name is unqualified (does not include the names of any containing "
    "namespaces), and the new procedure is created in the current namespace. If "
    "name includes any namespace qualifiers, the procedure is created in the "
    "specified namespace. The necessary namespaces are created automatically if "
    "they do not exist.\n\n"
    "The args parameter specifies the formal arguments to the procedure. It "
    "consists of a list, possibly empty, each of whose elements specifies one "
    "argument. Each argument specifier is also a list with either one or two "
    "fields. If there is only a single field in the specifier then it is the "
    "name of the argument; if there are two fields, then the first is the "
    "argument name and the second is its default value. Arguments with default "
    "values that are followed by non-defaulted arguments become required "
    "arguments; enough actual arguments must be supplied to allow all arguments "
    "up to and including the last required formal argument.\n\n"
    "When name is invoked a local variable will be created for each of the "
    "formal arguments to the procedure; its value will be the value of the "
    "corresponding argument in the invoking command or the argument's default "
    "value. Actual arguments are assigned to formal arguments strictly in order.\n\n"
    "There is one special case to permit procedures with variable numbers of "
    "arguments. If the last formal argument has the name args, then a call to "
    "the procedure may contain more actual arguments than the procedure has "
    "formal arguments. In this case, all of the actual arguments starting at "
    "the one that would be assigned to args are combined into a list; this "
    "combined value is assigned to the local variable args.\n\n"
    "When body is being executed, variable names normally refer to local "
    "variables, which are created automatically when referenced and deleted "
    "when the procedure returns. Other variables can only be accessed by "
    "invoking one of the global, variable, or upvar commands. The current "
    "namespace when body is executed will be the namespace that the procedure's "
    "name exists in.\n\n"
    "The proc command returns an empty string. When a procedure is invoked, "
    "the procedure's return value is the value specified in a return command. "
    "If the procedure does not execute an explicit return, then its return "
    "value is the value of the last command executed in the procedure's body. "
    "If an error occurs while executing the procedure body, then the "
    "procedure-as-a-whole will return that same error.");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<name>");
  e = feather_usage_help(ops, interp, e,
    "Name of the procedure. May be namespace-qualified (e.g., ::ns::myproc)");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<args>");
  e = feather_usage_help(ops, interp, e,
    "List of parameter specifiers. Each element is either a parameter name or "
    "{name default} for optional parameters");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_arg(ops, interp, "<body>");
  e = feather_usage_help(ops, interp, e,
    "Script to execute when the procedure is called");
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "proc greet {name} {\n"
    "    return \"Hello, $name!\"\n"
    "}",
    "Define a simple procedure with one parameter:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "proc add {a b} {\n"
    "    expr {$a + $b}\n"
    "}",
    "Procedure that returns result of last command:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "proc greet {{name \"World\"}} {\n"
    "    return \"Hello, $name!\"\n"
    "}",
    "Parameter with default value:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "proc sum {args} {\n"
    "    set total 0\n"
    "    foreach n $args {\n"
    "        set total [expr {$total + $n}]\n"
    "    }\n"
    "    return $total\n"
    "}",
    "Variadic procedure using args parameter:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "proc ::counter::incr {varName {amount 1}} {\n"
    "    upvar 1 $varName var\n"
    "    set var [expr {$var + $amount}]\n"
    "}",
    "Namespace-qualified procedure with optional parameter:",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_section(ops, interp, "See Also",
    "global, info, namespace, return, upvar, variable");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "proc", spec);
}
