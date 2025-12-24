#include "tclc.h"
#include "internal.h"

TclResult tcl_builtin_foreach(const TclHostOps *ops, TclInterp interp,
                               TclObj cmd, TclObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  // Need at least: varlist list body
  // Or: varlist1 list1 varlist2 list2 ... body
  // Minimum is 3 args, and must be odd (pairs + body)
  if (argc < 3 || (argc % 2) == 0) {
    TclObj msg = ops->string.intern(interp,
        "wrong # args: should be \"foreach varList list ?varList list ...? command\"", 73);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Number of varlist/list pairs
  size_t numPairs = (argc - 1) / 2;
  TclObj body = ops->list.at(interp, args, argc - 1);

  // Validate all varlists are non-empty
  for (size_t i = 0; i < numPairs; i++) {
    TclObj varlist = ops->list.at(interp, args, i * 2);
    TclObj varlistAsList = ops->list.from(interp, varlist);
    if (ops->list.length(interp, varlistAsList) == 0) {
      TclObj msg = ops->string.intern(interp, "foreach varlist is empty", 24);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
  }

  // Calculate maximum iterations needed across all lists
  size_t maxIters = 0;
  for (size_t i = 0; i < numPairs; i++) {
    TclObj varlist = ops->list.at(interp, args, i * 2);
    TclObj varlistAsList = ops->list.from(interp, varlist);
    size_t numVars = ops->list.length(interp, varlistAsList);

    TclObj list = ops->list.at(interp, args, i * 2 + 1);
    TclObj listAsList = ops->list.from(interp, list);
    size_t listLen = ops->list.length(interp, listAsList);

    // Number of iterations for this pair: ceiling of listLen / numVars
    size_t iters = (listLen + numVars - 1) / numVars;
    if (iters > maxIters) {
      maxIters = iters;
    }
  }

  // Iterate
  TclObj emptyStr = ops->string.intern(interp, "", 0);

  for (size_t iter = 0; iter < maxIters; iter++) {
    // Set variables for each varlist/list pair
    for (size_t pair = 0; pair < numPairs; pair++) {
      TclObj varlist = ops->list.at(interp, args, pair * 2);
      TclObj varlistAsList = ops->list.from(interp, varlist);
      size_t numVars = ops->list.length(interp, varlistAsList);

      TclObj list = ops->list.at(interp, args, pair * 2 + 1);
      TclObj listAsList = ops->list.from(interp, list);
      size_t listLen = ops->list.length(interp, listAsList);

      for (size_t v = 0; v < numVars; v++) {
        TclObj varName = ops->list.at(interp, varlistAsList, v);
        size_t listIdx = iter * numVars + v;
        TclObj value;
        if (listIdx < listLen) {
          value = ops->list.at(interp, listAsList, listIdx);
        } else {
          value = emptyStr;
        }
        ops->var.set(interp, varName, value);
      }
    }

    // Execute body
    TclResult rc = tcl_script_eval_obj(ops, interp, body, TCL_EVAL_LOCAL);

    if (rc == TCL_BREAK) {
      // break - exit loop normally
      break;
    } else if (rc == TCL_CONTINUE) {
      // continue - go to next iteration
      continue;
    } else if (rc != TCL_OK) {
      // Error - propagate
      return rc;
    }
  }

  // foreach returns empty string on normal completion
  ops->interp.set_result(interp, emptyStr);
  return TCL_OK;
}
