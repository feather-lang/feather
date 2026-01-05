#include "feather.h"
#include "internal.h"

FeatherResult feather_builtin_for(const FeatherHostOps *ops, FeatherInterp interp,
                          FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc != 4) {
    FeatherObj msg = ops->string.intern(interp,
        "wrong # args: should be \"for start test next command\"", 53);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj start = ops->list.at(interp, args, 0);
  FeatherObj test = ops->list.at(interp, args, 1);
  FeatherObj next = ops->list.at(interp, args, 2);
  FeatherObj body = ops->list.at(interp, args, 3);

  // Execute the init script
  FeatherResult rc = feather_script_eval_obj(ops, interp, start, TCL_EVAL_LOCAL);
  if (rc != TCL_OK) {
    return rc;
  }

  while (1) {
    // Evaluate condition
    int condResult;
    rc = feather_eval_bool_condition(ops, interp, test, &condResult);
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
      // continue was invoked - still execute 'next', then go to next iteration
    } else if (rc != TCL_OK) {
      // Error occurred - propagate
      return rc;
    }

    // Execute the 'next' script (increment/update)
    rc = feather_script_eval_obj(ops, interp, next, TCL_EVAL_LOCAL);
    if (rc == TCL_BREAK) {
      // break was invoked in next - exit loop normally
      break;
    } else if (rc != TCL_OK) {
      // Error or continue - propagate (continue in next is an error per TCL)
      return rc;
    }
  }

  // for returns empty string on normal completion
  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return TCL_OK;
}

void feather_register_for_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);

  // Command description (for NAME and DESCRIPTION sections)
  FeatherObj e = feather_usage_about(ops, interp,
    "Execute a C-style for loop",
    "The for command implements a loop similar to C-style for loops. It executes "
    "the start script once, then repeatedly evaluates the test expression. If test "
    "evaluates to true (non-zero), it executes the body script, then executes the "
    "next script, and repeats. The loop terminates when test evaluates to false "
    "(zero).\n\n"
    "The test argument is evaluated as an expression using the expr builtin. "
    "Integer values are treated as booleans (0 = false, non-zero = true).\n\n"
    "The for command returns an empty string upon normal completion. The break "
    "command can be used within body or next to exit the loop immediately. The "
    "continue command can be used within body to skip the rest of the current "
    "iteration while still executing the next script before the next iteration.");
  spec = feather_usage_add(ops, interp, spec, e);

  // Required argument: start
  e = feather_usage_arg(ops, interp, "<start>");
  e = feather_usage_help(ops, interp, e, "Script to execute once at the beginning of the loop");
  spec = feather_usage_add(ops, interp, spec, e);

  // Required argument: test
  e = feather_usage_arg(ops, interp, "<test>");
  e = feather_usage_help(ops, interp, e, "Expression to evaluate before each iteration (evaluated via expr)");
  spec = feather_usage_add(ops, interp, spec, e);

  // Required argument: next
  e = feather_usage_arg(ops, interp, "<next>");
  e = feather_usage_help(ops, interp, e, "Script to execute after each iteration of the body");
  spec = feather_usage_add(ops, interp, spec, e);

  // Required argument: body
  e = feather_usage_arg(ops, interp, "<body>");
  e = feather_usage_help(ops, interp, e, "Script to execute in each iteration");
  spec = feather_usage_add(ops, interp, spec, e);

  // Examples
  e = feather_usage_example(ops, interp,
    "for {set i 0} {$i < 10} {incr i} {\n"
    "    puts $i\n"
    "}",
    "Print numbers 0 through 9",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "for {set sum 0; set i 1} {$i <= 5} {incr i} {\n"
    "    set sum [expr {$sum + $i}]\n"
    "}",
    "Calculate the sum of integers 1 through 5",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "for", spec);
}
