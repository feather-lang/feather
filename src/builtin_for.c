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
    "'For' loop",
    "For is a looping command, similar in structure to the C for statement. "
    "The start, next, and body arguments must be Tcl command strings, and "
    "test is an expression string. The for command first invokes the Tcl "
    "interpreter to execute start. Then it repeatedly evaluates test as an "
    "expression; if the result is non-zero it invokes the Tcl interpreter on "
    "body, then invokes the Tcl interpreter on next, then repeats the loop. "
    "The command terminates when test evaluates to 0.\n\n"
    "If a continue command is invoked within body then any remaining commands "
    "in the current execution of body are skipped; processing continues by "
    "invoking the Tcl interpreter on next, then evaluating test, and so on. "
    "If a break command is invoked within body or next, then the for command "
    "will return immediately. The operation of break and continue are similar "
    "to the corresponding statements in C. For returns an empty string.\n\n"
    "Note that test should almost always be enclosed in braces. If not, "
    "variable substitutions will be made before the for command starts "
    "executing, which means that variable changes made by the loop body will "
    "not be considered in the expression. This is likely to result in an "
    "infinite loop. If test is enclosed in braces, variable substitutions "
    "are delayed until the expression is evaluated (before each loop "
    "iteration), so changes in the variables will be visible.");
  spec = feather_usage_add(ops, interp, spec, e);

  // Required argument: start
  e = feather_usage_arg(ops, interp, "<start>");
  e = feather_usage_help(ops, interp, e, "Script to execute once at the beginning");
  e = feather_usage_type(ops, interp, e, "script");
  spec = feather_usage_add(ops, interp, spec, e);

  // Required argument: test
  e = feather_usage_arg(ops, interp, "<test>");
  e = feather_usage_help(ops, interp, e, "Expression to evaluate before each iteration");
  spec = feather_usage_add(ops, interp, spec, e);

  // Required argument: next
  e = feather_usage_arg(ops, interp, "<next>");
  e = feather_usage_help(ops, interp, e, "Script to execute after each iteration");
  e = feather_usage_type(ops, interp, e, "script");
  spec = feather_usage_add(ops, interp, spec, e);

  // Required argument: body
  e = feather_usage_arg(ops, interp, "<body>");
  e = feather_usage_help(ops, interp, e, "Script to execute in each iteration");
  e = feather_usage_type(ops, interp, e, "script");
  spec = feather_usage_add(ops, interp, spec, e);

  // Examples
  e = feather_usage_example(ops, interp,
    "for {set x 0} {$x<10} {incr x} {\n"
    "    puts \"x is $x\"\n"
    "}",
    "Print a line for each of the integers from 0 to 9",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "for {set x 1} {$x<=1024} {set x [expr {$x * 2}]} {\n"
    "    puts \"x is $x\"\n"
    "}",
    "Print out the powers of two from 1 to 1024",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  // SEE ALSO section
  e = feather_usage_section(ops, interp, "See Also",
    "break, continue, foreach, while");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "for", spec);
}
