#include "feather.h"
#include "internal.h"

FeatherResult feather_builtin_lmap(const FeatherHostOps *ops, FeatherInterp interp,
                               FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  // Need at least: varlist list body
  // Or: varlist1 list1 varlist2 list2 ... body
  // Minimum is 3 args, and must be odd (pairs + body)
  if (argc < 3 || (argc % 2) == 0) {
    FeatherObj msg = ops->string.intern(interp,
        "wrong # args: should be \"lmap varList list ?varList list ...? command\"", 70);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  // Number of varlist/list pairs
  size_t numPairs = (argc - 1) / 2;
  FeatherObj body = ops->list.at(interp, args, argc - 1);

  // Validate all varlists are non-empty
  for (size_t i = 0; i < numPairs; i++) {
    FeatherObj varlist = ops->list.at(interp, args, i * 2);
    FeatherObj varlistAsList = ops->list.from(interp, varlist);
    if (ops->list.length(interp, varlistAsList) == 0) {
      FeatherObj msg = ops->string.intern(interp, "lmap varlist is empty", 21);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
  }

  // Calculate maximum iterations needed across all lists
  size_t maxIters = 0;
  for (size_t i = 0; i < numPairs; i++) {
    FeatherObj varlist = ops->list.at(interp, args, i * 2);
    FeatherObj varlistAsList = ops->list.from(interp, varlist);
    size_t numVars = ops->list.length(interp, varlistAsList);

    FeatherObj list = ops->list.at(interp, args, i * 2 + 1);
    FeatherObj listAsList = ops->list.from(interp, list);
    size_t listLen = ops->list.length(interp, listAsList);

    // Number of iterations for this pair: ceiling of listLen / numVars
    size_t iters = (listLen + numVars - 1) / numVars;
    if (iters > maxIters) {
      maxIters = iters;
    }
  }

  // Create accumulator list for results
  FeatherObj result = ops->list.create(interp);
  FeatherObj emptyStr = ops->string.intern(interp, "", 0);

  for (size_t iter = 0; iter < maxIters; iter++) {
    // Set variables for each varlist/list pair
    for (size_t pair = 0; pair < numPairs; pair++) {
      FeatherObj varlist = ops->list.at(interp, args, pair * 2);
      FeatherObj varlistAsList = ops->list.from(interp, varlist);
      size_t numVars = ops->list.length(interp, varlistAsList);

      FeatherObj list = ops->list.at(interp, args, pair * 2 + 1);
      FeatherObj listAsList = ops->list.from(interp, list);
      size_t listLen = ops->list.length(interp, listAsList);

      for (size_t v = 0; v < numVars; v++) {
        FeatherObj varName = ops->list.at(interp, varlistAsList, v);
        size_t listIdx = iter * numVars + v;
        FeatherObj value;
        if (listIdx < listLen) {
          value = ops->list.at(interp, listAsList, listIdx);
        } else {
          value = emptyStr;
        }
        ops->var.set(interp, varName, value);
      }
    }

    // Execute body
    FeatherResult rc = feather_script_eval_obj(ops, interp, body, TCL_EVAL_LOCAL);

    if (rc == TCL_BREAK) {
      // break - exit loop, return what we have so far
      break;
    } else if (rc == TCL_CONTINUE) {
      // continue - skip adding to result, go to next iteration
      continue;
    } else if (rc != TCL_OK) {
      // Error or return - propagate
      return rc;
    }

    // Append body result to accumulator
    FeatherObj bodyResult = ops->interp.get_result(interp);
    ops->list.push(interp, result, bodyResult);
  }

  // lmap returns the accumulated list
  ops->interp.set_result(interp, result);
  return TCL_OK;
}
