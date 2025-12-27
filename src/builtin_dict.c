#include "feather.h"
#include "internal.h"

// dict create ?key value ...?
static FeatherResult dict_create(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc % 2 != 0) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict create ?key value ...?\"", 53);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj dict = ops->dict.create(interp);
  while (ops->list.length(interp, args) >= 2) {
    FeatherObj key = ops->list.shift(interp, args);
    FeatherObj val = ops->list.shift(interp, args);
    dict = ops->dict.set(interp, dict, key, val);
  }

  ops->interp.set_result(interp, dict);
  return TCL_OK;
}

// dict get dictValue ?key ...?
static FeatherResult dict_get(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 1) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict get dictionary ?key ...?\"", 55);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj dict = ops->list.shift(interp, args);

  // If no keys, return dictionary as list (all key-value pairs)
  if (ops->list.length(interp, args) == 0) {
    ops->interp.set_result(interp, dict);
    return TCL_OK;
  }

  // Navigate through nested dicts
  while (ops->list.length(interp, args) > 0) {
    FeatherObj key = ops->list.shift(interp, args);
    FeatherObj val = ops->dict.get(interp, dict, key);
    if (ops->list.is_nil(interp, val)) {
      FeatherObj msg = ops->string.intern(interp, "key \"", 5);
      msg = ops->string.concat(interp, msg, key);
      FeatherObj suffix = ops->string.intern(interp, "\" not known in dictionary", 25);
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
static FeatherResult dict_set(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 3) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict set dictVarName key ?key ...? value\"", 66);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj varName = ops->list.shift(interp, args);

  // Get the current dict from the variable (or empty if doesn't exist)
  FeatherObj dict = ops->var.get(interp, varName);
  if (ops->list.is_nil(interp, dict)) {
    dict = ops->dict.create(interp);
  }

  // Get all keys except the last argument (which is the value)
  FeatherObj keys = ops->list.create(interp);
  while (ops->list.length(interp, args) > 1) {
    FeatherObj key = ops->list.shift(interp, args);
    keys = ops->list.push(interp, keys, key);
  }
  FeatherObj value = ops->list.shift(interp, args);

  size_t numKeys = ops->list.length(interp, keys);
  if (numKeys == 1) {
    // Simple case: single key
    FeatherObj key = ops->list.at(interp, keys, 0);
    dict = ops->dict.set(interp, dict, key, value);
  } else {
    // Nested dict case: navigate to innermost, then set
    // Build array of dicts from outermost to innermost
    FeatherObj dicts[64]; // Max nesting depth
    if (numKeys > 64) {
      FeatherObj msg = ops->string.intern(interp, "too many nested keys", 20);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }

    dicts[0] = dict;
    for (size_t i = 0; i < numKeys - 1; i++) {
      FeatherObj key = ops->list.at(interp, keys, i);
      FeatherObj nested = ops->dict.get(interp, dicts[i], key);
      if (ops->list.is_nil(interp, nested)) {
        nested = ops->dict.create(interp);
      }
      dicts[i + 1] = nested;
    }

    // Set value in innermost dict
    FeatherObj innerKey = ops->list.at(interp, keys, numKeys - 1);
    dicts[numKeys - 1] = ops->dict.set(interp, dicts[numKeys - 1], innerKey, value);

    // Rebuild from inside out
    for (size_t i = numKeys - 1; i > 0; i--) {
      FeatherObj key = ops->list.at(interp, keys, i - 1);
      dicts[i - 1] = ops->dict.set(interp, dicts[i - 1], key, dicts[i]);
    }
    dict = dicts[0];
  }

  ops->var.set(interp, varName, dict);
  ops->interp.set_result(interp, dict);
  return TCL_OK;
}

// dict exists dictValue key ?key ...?
static FeatherResult dict_exists(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict exists dictionary key ?key ...?\"", 62);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj dict = ops->list.shift(interp, args);

  // Navigate through nested dicts
  while (ops->list.length(interp, args) > 0) {
    FeatherObj key = ops->list.shift(interp, args);
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
static FeatherResult dict_keys(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 1 || argc > 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict keys dictionary ?globPattern?\"", 60);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj dict = ops->list.shift(interp, args);
  FeatherObj allKeys = ops->dict.keys(interp, dict);

  if (argc == 1) {
    // No pattern, return all keys
    ops->interp.set_result(interp, allKeys);
    return TCL_OK;
  }

  // Filter by pattern
  FeatherObj pattern = ops->list.shift(interp, args);

  FeatherObj result = ops->list.create(interp);
  size_t numKeys = ops->list.length(interp, allKeys);
  for (size_t i = 0; i < numKeys; i++) {
    FeatherObj key = ops->list.at(interp, allKeys, i);
    if (feather_obj_glob_match(ops, interp, pattern, key)) {
      result = ops->list.push(interp, result, key);
    }
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// dict values dictValue ?pattern?
static FeatherResult dict_values(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 1 || argc > 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict values dictionary ?globPattern?\"", 62);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj dict = ops->list.shift(interp, args);
  FeatherObj allValues = ops->dict.values(interp, dict);

  if (argc == 1) {
    // No pattern, return all values
    ops->interp.set_result(interp, allValues);
    return TCL_OK;
  }

  // Filter by pattern
  FeatherObj pattern = ops->list.shift(interp, args);

  FeatherObj result = ops->list.create(interp);
  size_t numVals = ops->list.length(interp, allValues);
  for (size_t i = 0; i < numVals; i++) {
    FeatherObj val = ops->list.at(interp, allValues, i);
    if (feather_obj_glob_match(ops, interp, pattern, val)) {
      result = ops->list.push(interp, result, val);
    }
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// dict size dictValue
static FeatherResult dict_size(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  if (ops->list.length(interp, args) != 1) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict size dictionary\"", 46);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj dict = ops->list.shift(interp, args);
  size_t sz = ops->dict.size(interp, dict);
  ops->interp.set_result(interp, ops->integer.create(interp, (int64_t)sz));
  return TCL_OK;
}

// dict remove dictValue ?key ...?
static FeatherResult dict_remove(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  if (ops->list.length(interp, args) < 1) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict remove dictionary ?key ...?\"", 58);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj dict = ops->list.shift(interp, args);

  // Remove each key
  while (ops->list.length(interp, args) > 0) {
    FeatherObj key = ops->list.shift(interp, args);
    dict = ops->dict.remove(interp, dict, key);
  }

  ops->interp.set_result(interp, dict);
  return TCL_OK;
}

// dict replace dictValue ?key value ...?
static FeatherResult dict_replace(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 1 || (argc - 1) % 2 != 0) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict replace dictionary ?key value ...?\"", 65);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj dict = ops->list.shift(interp, args);

  // Set each key-value pair
  while (ops->list.length(interp, args) >= 2) {
    FeatherObj key = ops->list.shift(interp, args);
    FeatherObj val = ops->list.shift(interp, args);
    dict = ops->dict.set(interp, dict, key, val);
  }

  ops->interp.set_result(interp, dict);
  return TCL_OK;
}

// dict merge ?dictValue ...?
static FeatherResult dict_merge(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  FeatherObj result = ops->dict.create(interp);

  while (ops->list.length(interp, args) > 0) {
    FeatherObj dict = ops->list.shift(interp, args);
    FeatherObj keys = ops->dict.keys(interp, dict);
    size_t numKeys = ops->list.length(interp, keys);
    for (size_t i = 0; i < numKeys; i++) {
      FeatherObj key = ops->list.at(interp, keys, i);
      FeatherObj val = ops->dict.get(interp, dict, key);
      result = ops->dict.set(interp, result, key, val);
    }
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// dict append dictVariable key ?string ...?
static FeatherResult dict_append(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict append dictVarName key ?value ...?\"", 65);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj varName = ops->list.shift(interp, args);
  FeatherObj key = ops->list.shift(interp, args);

  // Get current dict
  FeatherObj dict = ops->var.get(interp, varName);
  if (ops->list.is_nil(interp, dict)) {
    dict = ops->dict.create(interp);
  }

  // Get current value or empty string
  FeatherObj val = ops->dict.get(interp, dict, key);
  if (ops->list.is_nil(interp, val)) {
    val = ops->string.intern(interp, "", 0);
  }

  // Append all strings
  while (ops->list.length(interp, args) > 0) {
    FeatherObj str = ops->list.shift(interp, args);
    val = ops->string.concat(interp, val, str);
  }

  dict = ops->dict.set(interp, dict, key, val);
  ops->var.set(interp, varName, dict);
  ops->interp.set_result(interp, dict);
  return TCL_OK;
}

// dict incr dictVariable key ?increment?
static FeatherResult dict_incr(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 2 || argc > 3) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict incr dictVarName key ?increment?\"", 63);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj varName = ops->list.shift(interp, args);
  FeatherObj key = ops->list.shift(interp, args);

  int64_t increment = 1;
  if (argc == 3) {
    FeatherObj incrObj = ops->list.shift(interp, args);
    if (ops->integer.get(interp, incrObj, &increment) != TCL_OK) {
      FeatherObj msg = ops->string.intern(interp, "expected integer but got \"", 26);
      msg = ops->string.concat(interp, msg, incrObj);
      FeatherObj suffix = ops->string.intern(interp, "\"", 1);
      msg = ops->string.concat(interp, msg, suffix);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
  }

  // Get current dict
  FeatherObj dict = ops->var.get(interp, varName);
  if (ops->list.is_nil(interp, dict)) {
    dict = ops->dict.create(interp);
  }

  // Get current value or 0
  int64_t current = 0;
  FeatherObj val = ops->dict.get(interp, dict, key);
  if (!ops->list.is_nil(interp, val)) {
    if (ops->integer.get(interp, val, &current) != TCL_OK) {
      FeatherObj msg = ops->string.intern(interp,
        "expected integer but got \"", 26);
      msg = ops->string.concat(interp, msg, val);
      FeatherObj suffix = ops->string.intern(interp, "\"", 1);
      msg = ops->string.concat(interp, msg, suffix);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
  }

  current += increment;
  FeatherObj newVal = ops->integer.create(interp, current);
  dict = ops->dict.set(interp, dict, key, newVal);
  ops->var.set(interp, varName, dict);
  ops->interp.set_result(interp, dict);
  return TCL_OK;
}

// dict lappend dictVariable key ?value ...?
static FeatherResult dict_lappend(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict lappend dictVarName key ?value ...?\"", 66);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj varName = ops->list.shift(interp, args);
  FeatherObj key = ops->list.shift(interp, args);

  // Get current dict
  FeatherObj dict = ops->var.get(interp, varName);
  if (ops->list.is_nil(interp, dict)) {
    dict = ops->dict.create(interp);
  }

  // Get current value or empty list
  FeatherObj val = ops->dict.get(interp, dict, key);
  if (ops->list.is_nil(interp, val)) {
    val = ops->list.create(interp);
  } else {
    // Convert to list if needed (make mutable copy)
    val = ops->list.from(interp, val);
  }

  // Append all values
  while (ops->list.length(interp, args) > 0) {
    FeatherObj item = ops->list.shift(interp, args);
    val = ops->list.push(interp, val, item);
  }

  dict = ops->dict.set(interp, dict, key, val);
  ops->var.set(interp, varName, dict);
  ops->interp.set_result(interp, dict);
  return TCL_OK;
}

// dict unset dictVariable key ?key ...?
static FeatherResult dict_unset(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict unset dictVarName key ?key ...?\"", 62);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj varName = ops->list.shift(interp, args);

  // Get current dict
  FeatherObj dict = ops->var.get(interp, varName);
  if (ops->list.is_nil(interp, dict)) {
    dict = ops->dict.create(interp);
  }

  // Collect keys
  FeatherObj keys = ops->list.create(interp);
  while (ops->list.length(interp, args) > 0) {
    FeatherObj key = ops->list.shift(interp, args);
    keys = ops->list.push(interp, keys, key);
  }

  size_t numKeys = ops->list.length(interp, keys);
  if (numKeys == 1) {
    // Simple case
    FeatherObj key = ops->list.at(interp, keys, 0);
    dict = ops->dict.remove(interp, dict, key);
  } else {
    // Nested case: navigate and unset
    FeatherObj dicts[64];
    if (numKeys > 64) {
      FeatherObj msg = ops->string.intern(interp, "too many nested keys", 20);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }

    dicts[0] = dict;
    for (size_t i = 0; i < numKeys - 1; i++) {
      FeatherObj key = ops->list.at(interp, keys, i);
      FeatherObj nested = ops->dict.get(interp, dicts[i], key);
      if (ops->list.is_nil(interp, nested)) {
        // Key doesn't exist, nothing to unset
        ops->var.set(interp, varName, dict);
        ops->interp.set_result(interp, dict);
        return TCL_OK;
      }
      dicts[i + 1] = nested;
    }

    // Remove from innermost
    FeatherObj innerKey = ops->list.at(interp, keys, numKeys - 1);
    dicts[numKeys - 1] = ops->dict.remove(interp, dicts[numKeys - 1], innerKey);

    // Rebuild from inside out
    for (size_t i = numKeys - 1; i > 0; i--) {
      FeatherObj key = ops->list.at(interp, keys, i - 1);
      dicts[i - 1] = ops->dict.set(interp, dicts[i - 1], key, dicts[i]);
    }
    dict = dicts[0];
  }

  ops->var.set(interp, varName, dict);
  ops->interp.set_result(interp, dict);
  return TCL_OK;
}

// dict for {keyVar valueVar} dictValue body
static FeatherResult dict_for(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  if (ops->list.length(interp, args) != 3) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict for {keyVarName valueVarName} dictionary body\"", 77);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj varSpec = ops->list.shift(interp, args);
  FeatherObj dict = ops->list.shift(interp, args);
  FeatherObj body = ops->list.shift(interp, args);

  // Parse varSpec to get keyVar and valueVar
  FeatherObj varList = ops->list.from(interp, varSpec);
  if (ops->list.length(interp, varList) != 2) {
    FeatherObj msg = ops->string.intern(interp,
      "must have exactly two variable names", 36);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
  FeatherObj keyVar = ops->list.at(interp, varList, 0);
  FeatherObj valVar = ops->list.at(interp, varList, 1);

  FeatherObj keys = ops->dict.keys(interp, dict);
  size_t numKeys = ops->list.length(interp, keys);

  for (size_t i = 0; i < numKeys; i++) {
    FeatherObj key = ops->list.at(interp, keys, i);
    FeatherObj val = ops->dict.get(interp, dict, key);

    ops->var.set(interp, keyVar, key);
    ops->var.set(interp, valVar, val);

    FeatherResult res = feather_script_eval_obj(ops, interp, body, TCL_EVAL_LOCAL);
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
static FeatherResult dict_info(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  if (ops->list.length(interp, args) != 1) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict info dictionary\"", 46);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj dict = ops->list.shift(interp, args);
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
static FeatherResult dict_getdef(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 3) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict getdef dictionary ?key ...? key default\"", 70);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj dict = ops->list.shift(interp, args);

  // Last argument is default, rest are keys
  FeatherObj defaultVal = ops->list.pop(interp, args);

  // Navigate through nested dicts
  while (ops->list.length(interp, args) > 0) {
    FeatherObj key = ops->list.shift(interp, args);
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
FeatherResult feather_builtin_dict(const FeatherHostOps *ops, FeatherInterp interp,
                           FeatherObj cmd, FeatherObj args) {
  (void)cmd;
  size_t argc = ops->list.length(interp, args);

  if (argc < 1) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict subcommand ?arg ...?\"", 51);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj subcmd = ops->list.shift(interp, args);

  if (feather_obj_eq_literal(ops, interp, subcmd, "create")) {
    return dict_create(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "get")) {
    return dict_get(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "set")) {
    return dict_set(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "exists")) {
    return dict_exists(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "keys")) {
    return dict_keys(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "values")) {
    return dict_values(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "size")) {
    return dict_size(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "remove")) {
    return dict_remove(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "replace")) {
    return dict_replace(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "merge")) {
    return dict_merge(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "append")) {
    return dict_append(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "incr")) {
    return dict_incr(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "lappend")) {
    return dict_lappend(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "unset")) {
    return dict_unset(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "for")) {
    return dict_for(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "info")) {
    return dict_info(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "getdef") || feather_obj_eq_literal(ops, interp, subcmd, "getwithdefault")) {
    return dict_getdef(ops, interp, args);
  } else {
    FeatherObj msg = ops->string.intern(interp, "unknown or ambiguous subcommand \"", 33);
    msg = ops->string.concat(interp, msg, subcmd);
    FeatherObj suffix = ops->string.intern(interp,
      "\": must be append, create, exists, for, get, getdef, getwithdefault, incr, info, keys, lappend, merge, remove, replace, set, size, unset, or values", 147);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
}
