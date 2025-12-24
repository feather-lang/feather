#include "tclc.h"
#include "internal.h"

// Helper to check string equality
static int str_eq(const char *s, size_t len, const char *lit) {
  size_t llen = 0;
  while (lit[llen]) llen++;
  if (len != llen) return 0;
  for (size_t i = 0; i < len; i++) {
    if (s[i] != lit[i]) return 0;
  }
  return 1;
}

// dict create ?key value ...?
static TclResult dict_create(const TclHostOps *ops, TclInterp interp, TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc % 2 != 0) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict create ?key value ...?\"", 53);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj dict = ops->dict.create(interp);
  while (ops->list.length(interp, args) >= 2) {
    TclObj key = ops->list.shift(interp, args);
    TclObj val = ops->list.shift(interp, args);
    dict = ops->dict.set(interp, dict, key, val);
  }

  ops->interp.set_result(interp, dict);
  return TCL_OK;
}

// dict get dictValue ?key ...?
static TclResult dict_get(const TclHostOps *ops, TclInterp interp, TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 1) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict get dictionary ?key ...?\"", 55);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj dict = ops->list.shift(interp, args);

  // If no keys, return dictionary as list (all key-value pairs)
  if (ops->list.length(interp, args) == 0) {
    ops->interp.set_result(interp, dict);
    return TCL_OK;
  }

  // Navigate through nested dicts
  while (ops->list.length(interp, args) > 0) {
    TclObj key = ops->list.shift(interp, args);
    TclObj val = ops->dict.get(interp, dict, key);
    if (ops->list.is_nil(interp, val)) {
      TclObj msg = ops->string.intern(interp, "key \"", 5);
      msg = ops->string.concat(interp, msg, key);
      TclObj suffix = ops->string.intern(interp, "\" not known in dictionary", 25);
      msg = ops->string.concat(interp, msg, suffix);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
    dict = val;
  }

  ops->interp.set_result(interp, dict);
  return TCL_OK;
}

// dict set dictVariable key ?key ...? value
static TclResult dict_set(const TclHostOps *ops, TclInterp interp, TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 3) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict set dictVarName key ?key ...? value\"", 66);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj varName = ops->list.shift(interp, args);

  // Get the current dict from the variable (or empty if doesn't exist)
  TclObj dict = ops->var.get(interp, varName);
  if (ops->list.is_nil(interp, dict)) {
    dict = ops->dict.create(interp);
  }

  // Get all keys except the last argument (which is the value)
  TclObj keys = ops->list.create(interp);
  while (ops->list.length(interp, args) > 1) {
    TclObj key = ops->list.shift(interp, args);
    keys = ops->list.push(interp, keys, key);
  }
  TclObj value = ops->list.shift(interp, args);

  size_t numKeys = ops->list.length(interp, keys);
  if (numKeys == 1) {
    // Simple case: single key
    TclObj key = ops->list.at(interp, keys, 0);
    dict = ops->dict.set(interp, dict, key, value);
  } else {
    // Nested dict case: navigate to innermost, then set
    // Build array of dicts from outermost to innermost
    TclObj dicts[64]; // Max nesting depth
    if (numKeys > 64) {
      TclObj msg = ops->string.intern(interp, "too many nested keys", 20);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }

    dicts[0] = dict;
    for (size_t i = 0; i < numKeys - 1; i++) {
      TclObj key = ops->list.at(interp, keys, i);
      TclObj nested = ops->dict.get(interp, dicts[i], key);
      if (ops->list.is_nil(interp, nested)) {
        nested = ops->dict.create(interp);
      }
      dicts[i + 1] = nested;
    }

    // Set value in innermost dict
    TclObj innerKey = ops->list.at(interp, keys, numKeys - 1);
    dicts[numKeys - 1] = ops->dict.set(interp, dicts[numKeys - 1], innerKey, value);

    // Rebuild from inside out
    for (size_t i = numKeys - 1; i > 0; i--) {
      TclObj key = ops->list.at(interp, keys, i - 1);
      dicts[i - 1] = ops->dict.set(interp, dicts[i - 1], key, dicts[i]);
    }
    dict = dicts[0];
  }

  ops->var.set(interp, varName, dict);
  ops->interp.set_result(interp, dict);
  return TCL_OK;
}

// dict exists dictValue key ?key ...?
static TclResult dict_exists(const TclHostOps *ops, TclInterp interp, TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 2) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict exists dictionary key ?key ...?\"", 62);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj dict = ops->list.shift(interp, args);

  // Navigate through nested dicts
  while (ops->list.length(interp, args) > 0) {
    TclObj key = ops->list.shift(interp, args);
    if (!ops->dict.exists(interp, dict, key)) {
      ops->interp.set_result(interp, ops->integer.create(interp, 0));
      return TCL_OK;
    }
    dict = ops->dict.get(interp, dict, key);
  }

  ops->interp.set_result(interp, ops->integer.create(interp, 1));
  return TCL_OK;
}

// dict keys dictValue ?pattern?
static TclResult dict_keys(const TclHostOps *ops, TclInterp interp, TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 1 || argc > 2) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict keys dictionary ?globPattern?\"", 60);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj dict = ops->list.shift(interp, args);
  TclObj allKeys = ops->dict.keys(interp, dict);

  if (argc == 1) {
    // No pattern, return all keys
    ops->interp.set_result(interp, allKeys);
    return TCL_OK;
  }

  // Filter by pattern
  TclObj pattern = ops->list.shift(interp, args);
  size_t patLen;
  const char *patStr = ops->string.get(interp, pattern, &patLen);

  TclObj result = ops->list.create(interp);
  size_t numKeys = ops->list.length(interp, allKeys);
  for (size_t i = 0; i < numKeys; i++) {
    TclObj key = ops->list.at(interp, allKeys, i);
    size_t keyLen;
    const char *keyStr = ops->string.get(interp, key, &keyLen);
    if (tcl_glob_match(patStr, patLen, keyStr, keyLen)) {
      result = ops->list.push(interp, result, key);
    }
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// dict values dictValue ?pattern?
static TclResult dict_values(const TclHostOps *ops, TclInterp interp, TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 1 || argc > 2) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict values dictionary ?globPattern?\"", 62);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj dict = ops->list.shift(interp, args);
  TclObj allValues = ops->dict.values(interp, dict);

  if (argc == 1) {
    // No pattern, return all values
    ops->interp.set_result(interp, allValues);
    return TCL_OK;
  }

  // Filter by pattern
  TclObj pattern = ops->list.shift(interp, args);
  size_t patLen;
  const char *patStr = ops->string.get(interp, pattern, &patLen);

  TclObj result = ops->list.create(interp);
  size_t numVals = ops->list.length(interp, allValues);
  for (size_t i = 0; i < numVals; i++) {
    TclObj val = ops->list.at(interp, allValues, i);
    size_t valLen;
    const char *valStr = ops->string.get(interp, val, &valLen);
    if (tcl_glob_match(patStr, patLen, valStr, valLen)) {
      result = ops->list.push(interp, result, val);
    }
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// dict size dictValue
static TclResult dict_size(const TclHostOps *ops, TclInterp interp, TclObj args) {
  if (ops->list.length(interp, args) != 1) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict size dictionary\"", 46);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj dict = ops->list.shift(interp, args);
  size_t sz = ops->dict.size(interp, dict);
  ops->interp.set_result(interp, ops->integer.create(interp, (int64_t)sz));
  return TCL_OK;
}

// dict remove dictValue ?key ...?
static TclResult dict_remove(const TclHostOps *ops, TclInterp interp, TclObj args) {
  if (ops->list.length(interp, args) < 1) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict remove dictionary ?key ...?\"", 58);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj dict = ops->list.shift(interp, args);

  // Remove each key
  while (ops->list.length(interp, args) > 0) {
    TclObj key = ops->list.shift(interp, args);
    dict = ops->dict.remove(interp, dict, key);
  }

  ops->interp.set_result(interp, dict);
  return TCL_OK;
}

// dict replace dictValue ?key value ...?
static TclResult dict_replace(const TclHostOps *ops, TclInterp interp, TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 1 || (argc - 1) % 2 != 0) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict replace dictionary ?key value ...?\"", 65);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj dict = ops->list.shift(interp, args);

  // Set each key-value pair
  while (ops->list.length(interp, args) >= 2) {
    TclObj key = ops->list.shift(interp, args);
    TclObj val = ops->list.shift(interp, args);
    dict = ops->dict.set(interp, dict, key, val);
  }

  ops->interp.set_result(interp, dict);
  return TCL_OK;
}

// dict merge ?dictValue ...?
static TclResult dict_merge(const TclHostOps *ops, TclInterp interp, TclObj args) {
  TclObj result = ops->dict.create(interp);

  while (ops->list.length(interp, args) > 0) {
    TclObj dict = ops->list.shift(interp, args);
    TclObj keys = ops->dict.keys(interp, dict);
    size_t numKeys = ops->list.length(interp, keys);
    for (size_t i = 0; i < numKeys; i++) {
      TclObj key = ops->list.at(interp, keys, i);
      TclObj val = ops->dict.get(interp, dict, key);
      result = ops->dict.set(interp, result, key, val);
    }
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// dict append dictVariable key ?string ...?
static TclResult dict_append(const TclHostOps *ops, TclInterp interp, TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 2) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict append dictVarName key ?value ...?\"", 65);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj varName = ops->list.shift(interp, args);
  TclObj key = ops->list.shift(interp, args);

  // Get current dict
  TclObj dict = ops->var.get(interp, varName);
  if (ops->list.is_nil(interp, dict)) {
    dict = ops->dict.create(interp);
  }

  // Get current value or empty string
  TclObj val = ops->dict.get(interp, dict, key);
  if (ops->list.is_nil(interp, val)) {
    val = ops->string.intern(interp, "", 0);
  }

  // Append all strings
  while (ops->list.length(interp, args) > 0) {
    TclObj str = ops->list.shift(interp, args);
    val = ops->string.concat(interp, val, str);
  }

  dict = ops->dict.set(interp, dict, key, val);
  ops->var.set(interp, varName, dict);
  ops->interp.set_result(interp, dict);
  return TCL_OK;
}

// dict incr dictVariable key ?increment?
static TclResult dict_incr(const TclHostOps *ops, TclInterp interp, TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 2 || argc > 3) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict incr dictVarName key ?increment?\"", 63);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj varName = ops->list.shift(interp, args);
  TclObj key = ops->list.shift(interp, args);

  int64_t increment = 1;
  if (argc == 3) {
    TclObj incrObj = ops->list.shift(interp, args);
    if (ops->integer.get(interp, incrObj, &increment) != TCL_OK) {
      TclObj msg = ops->string.intern(interp, "expected integer but got \"", 26);
      msg = ops->string.concat(interp, msg, incrObj);
      TclObj suffix = ops->string.intern(interp, "\"", 1);
      msg = ops->string.concat(interp, msg, suffix);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
  }

  // Get current dict
  TclObj dict = ops->var.get(interp, varName);
  if (ops->list.is_nil(interp, dict)) {
    dict = ops->dict.create(interp);
  }

  // Get current value or 0
  int64_t current = 0;
  TclObj val = ops->dict.get(interp, dict, key);
  if (!ops->list.is_nil(interp, val)) {
    if (ops->integer.get(interp, val, &current) != TCL_OK) {
      TclObj msg = ops->string.intern(interp,
        "expected integer but got \"", 26);
      msg = ops->string.concat(interp, msg, val);
      TclObj suffix = ops->string.intern(interp, "\"", 1);
      msg = ops->string.concat(interp, msg, suffix);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
  }

  current += increment;
  TclObj newVal = ops->integer.create(interp, current);
  dict = ops->dict.set(interp, dict, key, newVal);
  ops->var.set(interp, varName, dict);
  ops->interp.set_result(interp, dict);
  return TCL_OK;
}

// dict lappend dictVariable key ?value ...?
static TclResult dict_lappend(const TclHostOps *ops, TclInterp interp, TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 2) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict lappend dictVarName key ?value ...?\"", 66);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj varName = ops->list.shift(interp, args);
  TclObj key = ops->list.shift(interp, args);

  // Get current dict
  TclObj dict = ops->var.get(interp, varName);
  if (ops->list.is_nil(interp, dict)) {
    dict = ops->dict.create(interp);
  }

  // Get current value or empty list
  TclObj val = ops->dict.get(interp, dict, key);
  if (ops->list.is_nil(interp, val)) {
    val = ops->list.create(interp);
  } else {
    // Convert to list if needed (make mutable copy)
    val = ops->list.from(interp, val);
  }

  // Append all values
  while (ops->list.length(interp, args) > 0) {
    TclObj item = ops->list.shift(interp, args);
    val = ops->list.push(interp, val, item);
  }

  dict = ops->dict.set(interp, dict, key, val);
  ops->var.set(interp, varName, dict);
  ops->interp.set_result(interp, dict);
  return TCL_OK;
}

// dict unset dictVariable key ?key ...?
static TclResult dict_unset(const TclHostOps *ops, TclInterp interp, TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 2) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict unset dictVarName key ?key ...?\"", 62);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj varName = ops->list.shift(interp, args);

  // Get current dict
  TclObj dict = ops->var.get(interp, varName);
  if (ops->list.is_nil(interp, dict)) {
    dict = ops->dict.create(interp);
  }

  // Collect keys
  TclObj keys = ops->list.create(interp);
  while (ops->list.length(interp, args) > 0) {
    TclObj key = ops->list.shift(interp, args);
    keys = ops->list.push(interp, keys, key);
  }

  size_t numKeys = ops->list.length(interp, keys);
  if (numKeys == 1) {
    // Simple case
    TclObj key = ops->list.at(interp, keys, 0);
    dict = ops->dict.remove(interp, dict, key);
  } else {
    // Nested case: navigate and unset
    TclObj dicts[64];
    if (numKeys > 64) {
      TclObj msg = ops->string.intern(interp, "too many nested keys", 20);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }

    dicts[0] = dict;
    for (size_t i = 0; i < numKeys - 1; i++) {
      TclObj key = ops->list.at(interp, keys, i);
      TclObj nested = ops->dict.get(interp, dicts[i], key);
      if (ops->list.is_nil(interp, nested)) {
        // Key doesn't exist, nothing to unset
        ops->var.set(interp, varName, dict);
        ops->interp.set_result(interp, dict);
        return TCL_OK;
      }
      dicts[i + 1] = nested;
    }

    // Remove from innermost
    TclObj innerKey = ops->list.at(interp, keys, numKeys - 1);
    dicts[numKeys - 1] = ops->dict.remove(interp, dicts[numKeys - 1], innerKey);

    // Rebuild from inside out
    for (size_t i = numKeys - 1; i > 0; i--) {
      TclObj key = ops->list.at(interp, keys, i - 1);
      dicts[i - 1] = ops->dict.set(interp, dicts[i - 1], key, dicts[i]);
    }
    dict = dicts[0];
  }

  ops->var.set(interp, varName, dict);
  ops->interp.set_result(interp, dict);
  return TCL_OK;
}

// dict for {keyVar valueVar} dictValue body
static TclResult dict_for(const TclHostOps *ops, TclInterp interp, TclObj args) {
  if (ops->list.length(interp, args) != 3) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict for {keyVarName valueVarName} dictionary body\"", 77);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj varSpec = ops->list.shift(interp, args);
  TclObj dict = ops->list.shift(interp, args);
  TclObj body = ops->list.shift(interp, args);

  // Parse varSpec to get keyVar and valueVar
  TclObj varList = ops->list.from(interp, varSpec);
  if (ops->list.length(interp, varList) != 2) {
    TclObj msg = ops->string.intern(interp,
      "must have exactly two variable names", 36);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
  TclObj keyVar = ops->list.at(interp, varList, 0);
  TclObj valVar = ops->list.at(interp, varList, 1);

  TclObj keys = ops->dict.keys(interp, dict);
  size_t numKeys = ops->list.length(interp, keys);

  for (size_t i = 0; i < numKeys; i++) {
    TclObj key = ops->list.at(interp, keys, i);
    TclObj val = ops->dict.get(interp, dict, key);

    ops->var.set(interp, keyVar, key);
    ops->var.set(interp, valVar, val);

    TclResult res = tcl_script_eval_obj(ops, interp, body, TCL_EVAL_LOCAL);
    if (res == TCL_BREAK) {
      break;
    } else if (res == TCL_CONTINUE) {
      continue;
    } else if (res != TCL_OK) {
      return res;
    }
  }

  ops->interp.set_result(interp, ops->string.intern(interp, "", 0));
  return TCL_OK;
}

// dict info dictValue
static TclResult dict_info(const TclHostOps *ops, TclInterp interp, TclObj args) {
  if (ops->list.length(interp, args) != 1) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict info dictionary\"", 46);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj dict = ops->list.shift(interp, args);
  size_t sz = ops->dict.size(interp, dict);

  // Return simple info string
  char buf[128];
  int len = 0;
  // Simple implementation: just report size
  int64_t n = (int64_t)sz;
  if (n == 0) {
    buf[len++] = '0';
  } else {
    char tmp[32];
    int tl = 0;
    while (n > 0) {
      tmp[tl++] = '0' + (n % 10);
      n /= 10;
    }
    while (tl > 0) {
      buf[len++] = tmp[--tl];
    }
  }
  const char *suffix = " entries in table";
  for (int i = 0; suffix[i]; i++) {
    buf[len++] = suffix[i];
  }

  ops->interp.set_result(interp, ops->string.intern(interp, buf, len));
  return TCL_OK;
}

// dict getdef / getwithdefault dictValue ?key ...? key default
static TclResult dict_getdef(const TclHostOps *ops, TclInterp interp, TclObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 3) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict getdef dictionary ?key ...? key default\"", 70);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj dict = ops->list.shift(interp, args);

  // Last argument is default, rest are keys
  TclObj defaultVal = ops->list.pop(interp, args);

  // Navigate through nested dicts
  while (ops->list.length(interp, args) > 0) {
    TclObj key = ops->list.shift(interp, args);
    if (!ops->dict.exists(interp, dict, key)) {
      ops->interp.set_result(interp, defaultVal);
      return TCL_OK;
    }
    dict = ops->dict.get(interp, dict, key);
  }

  ops->interp.set_result(interp, dict);
  return TCL_OK;
}

// Main dict command dispatcher
TclResult tcl_builtin_dict(const TclHostOps *ops, TclInterp interp,
                           TclObj cmd, TclObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1) {
    TclObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict subcommand ?arg ...?\"", 51);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  TclObj subcmd = ops->list.shift(interp, args);
  size_t len;
  const char *subcmdStr = ops->string.get(interp, subcmd, &len);

  if (str_eq(subcmdStr, len, "create")) {
    return dict_create(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "get")) {
    return dict_get(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "set")) {
    return dict_set(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "exists")) {
    return dict_exists(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "keys")) {
    return dict_keys(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "values")) {
    return dict_values(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "size")) {
    return dict_size(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "remove")) {
    return dict_remove(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "replace")) {
    return dict_replace(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "merge")) {
    return dict_merge(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "append")) {
    return dict_append(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "incr")) {
    return dict_incr(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "lappend")) {
    return dict_lappend(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "unset")) {
    return dict_unset(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "for")) {
    return dict_for(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "info")) {
    return dict_info(ops, interp, args);
  } else if (str_eq(subcmdStr, len, "getdef") || str_eq(subcmdStr, len, "getwithdefault")) {
    return dict_getdef(ops, interp, args);
  } else {
    TclObj msg = ops->string.intern(interp, "unknown or ambiguous subcommand \"", 33);
    msg = ops->string.concat(interp, msg, subcmd);
    TclObj suffix = ops->string.intern(interp,
      "\": must be append, create, exists, for, get, getdef, getwithdefault, incr, info, keys, lappend, merge, remove, replace, set, size, unset, or values", 147);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
}
