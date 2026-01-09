#include "feather.h"
#include "internal.h"

FeatherResult feather_builtin_while(const FeatherHostOps *ops, FeatherInterp interp,
                             FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc != 2) {
    FeatherObj msg = ops->string.intern(interp, "wrong # args: should be \"while test command\"", 44);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj condition = ops->list.shift(interp, args);
  FeatherObj body = ops->list.shift(interp, args);

  while (1) {
    // Evaluate condition
    int condResult;
    FeatherResult rc = feather_eval_bool_condition(ops, interp, condition, &condResult);
    if (rc != TCL_OK) {
      return rc;
    }

    if (!condResult) {
      // Condition is false, exit loop
      break;
    }

    // Execute body as a script
    rc = feather_script_eval_obj(ops, interp, body, TCL_EVAL_LOCAL);

    if (rc == TCL_BREAK) {
      // break was invoked - exit loop normally
      break;
    } else if (rc == TCL_CONTINUE) {
      // continue was invoked - go to next iteration
      continue;
    } else if (rc != TCL_OK) {
      // Error occurred - propagate
      return rc;
    }
  }

  // while returns empty string on normal completion
  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return TCL_OK;
}

void feather_register_while_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  // Command description (for NAME and DESCRIPTION sections)
  FeatherObj e = feather_usage_about(ops, interp,
    "Execute script repeatedly as long as a condition is met",
    "The while command evaluates test as an expression (in the same way that "
    "expr evaluates its argument). The value of the expression must be a "
    "proper boolean value; if it is a true value then body is executed by "
    "passing it to the Tcl interpreter. Once body has been executed then "
    "test is evaluated again, and the process repeats until eventually test "
    "evaluates to a false boolean value. Continue commands may be executed "
    "inside body to terminate the current iteration of the loop, and break "
    "commands may be executed inside body to cause immediate termination of "
    "the while command. The while command always returns an empty string.\n\n"
    "Note that test should almost always be enclosed in braces. If not, "
    "variable substitutions will be made before the while command starts "
    "executing, which means that variable changes made by the loop body will "
    "not be considered in the expression. This is likely to result in an "
    "infinite loop. If test is enclosed in braces, variable substitutions "
    "are delayed until the expression is evaluated (before each loop "
    "iteration), so changes in the variables will be visible.");
  spec = feather_usage_add(ops, interp, spec, e);

  // Required argument: test
  e = feather_usage_arg(ops, interp, "<test>");
  e = feather_usage_help(ops, interp, e, "Boolean expression to evaluate before each iteration");
  spec = feather_usage_add(ops, interp, spec, e);

  // Required argument: body
  e = feather_usage_arg(ops, interp, "<body>");
  e = feather_usage_help(ops, interp, e, "Script to execute while test is true");
  e = feather_usage_type(ops, interp, e, "script");
  spec = feather_usage_add(ops, interp, spec, e);

  // Examples
  e = feather_usage_example(ops, interp,
    "set x 0\n"
    "while {$x < 5} {\n"
    "    puts $x\n"
    "    incr x\n"
    "}",
    "Print numbers 0 through 4",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "set i 10\n"
    "while {$i > 0} {\n"
    "    if {$i == 5} {\n"
    "        break\n"
    "    }\n"
    "    incr i -1\n"
    "}",
    "Loop exits early when i equals 5 using break",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "set i 0\n"
    "while {$i < 10} {\n"
    "    incr i\n"
    "    if {$i % 2 == 0} {\n"
    "        continue\n"
    "    }\n"
    "    puts $i\n"
    "}",
    "Print only odd numbers using continue",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  // SEE ALSO section
  e = feather_usage_section(ops, interp, "See Also",
    "break, continue, for, foreach");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "while", spec);
}
