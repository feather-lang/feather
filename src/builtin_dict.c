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
  FeatherObj dict;
  feather_get_var(ops, interp, varName, &dict);
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

  if (feather_set_var(ops, interp, varName, dict) != TCL_OK) {
    return TCL_ERROR;
  }
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
  FeatherObj dict;
  feather_get_var(ops, interp, varName, &dict);
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
  if (feather_set_var(ops, interp, varName, dict) != TCL_OK) {
    return TCL_ERROR;
  }
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
      feather_error_expected(ops, interp, "integer", incrObj);
      return TCL_ERROR;
    }
  }

  // Get current dict
  FeatherObj dict;
  feather_get_var(ops, interp, varName, &dict);
  if (ops->list.is_nil(interp, dict)) {
    dict = ops->dict.create(interp);
  }

  // Get current value or 0
  int64_t current = 0;
  FeatherObj val = ops->dict.get(interp, dict, key);
  if (!ops->list.is_nil(interp, val)) {
    if (ops->integer.get(interp, val, &current) != TCL_OK) {
      feather_error_expected(ops, interp, "integer", val);
      return TCL_ERROR;
    }
  }

  current += increment;
  FeatherObj newVal = ops->integer.create(interp, current);
  dict = ops->dict.set(interp, dict, key, newVal);
  if (feather_set_var(ops, interp, varName, dict) != TCL_OK) {
    return TCL_ERROR;
  }
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
  FeatherObj dict;
  feather_get_var(ops, interp, varName, &dict);
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
  if (feather_set_var(ops, interp, varName, dict) != TCL_OK) {
    return TCL_ERROR;
  }
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
  FeatherObj dict;
  feather_get_var(ops, interp, varName, &dict);
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
        if (feather_set_var(ops, interp, varName, dict) != TCL_OK) {
          return TCL_ERROR;
        }
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

  if (feather_set_var(ops, interp, varName, dict) != TCL_OK) {
    return TCL_ERROR;
  }
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

    if (feather_set_var(ops, interp, keyVar, key) != TCL_OK) {
      return TCL_ERROR;
    }
    if (feather_set_var(ops, interp, valVar, val) != TCL_OK) {
      return TCL_ERROR;
    }

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

// dict filter dictionary filterType ?arg ...?
static FeatherResult dict_filter(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict filter dictionary filterType ?arg ...?\"", 69);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj dict = ops->list.shift(interp, args);
  FeatherObj filterType = ops->list.shift(interp, args);

  FeatherObj result = ops->dict.create(interp);
  FeatherObj keys = ops->dict.keys(interp, dict);
  size_t numKeys = ops->list.length(interp, keys);

  if (feather_obj_eq_literal(ops, interp, filterType, "key")) {
    // dict filter dictionary key ?pattern ...?
    for (size_t i = 0; i < numKeys; i++) {
      FeatherObj key = ops->list.at(interp, keys, i);
      // Check if key matches any pattern
      int matched = 0;
      size_t numPatterns = ops->list.length(interp, args);
      if (numPatterns == 0) {
        matched = 1; // No patterns means match all
      } else {
        for (size_t p = 0; p < numPatterns; p++) {
          FeatherObj pattern = ops->list.at(interp, args, p);
          if (feather_obj_glob_match(ops, interp, pattern, key)) {
            matched = 1;
            break;
          }
        }
      }
      if (matched) {
        FeatherObj val = ops->dict.get(interp, dict, key);
        result = ops->dict.set(interp, result, key, val);
      }
    }
  } else if (feather_obj_eq_literal(ops, interp, filterType, "value")) {
    // dict filter dictionary value ?pattern ...?
    for (size_t i = 0; i < numKeys; i++) {
      FeatherObj key = ops->list.at(interp, keys, i);
      FeatherObj val = ops->dict.get(interp, dict, key);
      // Check if value matches any pattern
      int matched = 0;
      size_t numPatterns = ops->list.length(interp, args);
      if (numPatterns == 0) {
        matched = 1;
      } else {
        for (size_t p = 0; p < numPatterns; p++) {
          FeatherObj pattern = ops->list.at(interp, args, p);
          if (feather_obj_glob_match(ops, interp, pattern, val)) {
            matched = 1;
            break;
          }
        }
      }
      if (matched) {
        result = ops->dict.set(interp, result, key, val);
      }
    }
  } else if (feather_obj_eq_literal(ops, interp, filterType, "script")) {
    // dict filter dictionary script {keyVar valueVar} script
    if (ops->list.length(interp, args) != 2) {
      FeatherObj msg = ops->string.intern(interp,
        "wrong # args: should be \"dict filter dictionary script {keyVarName valueVarName} filterScript\"", 94);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }

    FeatherObj varSpec = ops->list.shift(interp, args);
    FeatherObj script = ops->list.shift(interp, args);

    FeatherObj varList = ops->list.from(interp, varSpec);
    if (ops->list.length(interp, varList) != 2) {
      FeatherObj msg = ops->string.intern(interp, "must have exactly two variable names", 36);
      ops->interp.set_result(interp, msg);
      return TCL_ERROR;
    }
    FeatherObj keyVar = ops->list.at(interp, varList, 0);
    FeatherObj valVar = ops->list.at(interp, varList, 1);

    for (size_t i = 0; i < numKeys; i++) {
      FeatherObj key = ops->list.at(interp, keys, i);
      FeatherObj val = ops->dict.get(interp, dict, key);

      if (feather_set_var(ops, interp, keyVar, key) != TCL_OK) {
        return TCL_ERROR;
      }
      if (feather_set_var(ops, interp, valVar, val) != TCL_OK) {
        return TCL_ERROR;
      }

      FeatherResult res = feather_script_eval_obj(ops, interp, script, TCL_EVAL_LOCAL);
      if (res == TCL_BREAK) {
        break;
      } else if (res == TCL_CONTINUE) {
        continue;
      } else if (res != TCL_OK) {
        return res;
      }

      // Check if result is true
      FeatherObj scriptResult = ops->interp.get_result(interp);
      int boolVal;
      if (feather_obj_to_bool_literal(ops, interp, scriptResult, &boolVal)) {
        if (boolVal) {
          result = ops->dict.set(interp, result, key, val);
        }
      } else {
        int64_t intVal;
        if (ops->integer.get(interp, scriptResult, &intVal) == TCL_OK) {
          if (intVal != 0) {
            result = ops->dict.set(interp, result, key, val);
          }
        }
      }
    }
  } else {
    FeatherObj msg = ops->string.intern(interp, "bad filterType \"", 16);
    msg = ops->string.concat(interp, msg, filterType);
    FeatherObj suffix = ops->string.intern(interp, "\": must be key, script, or value", 32);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// dict map {keyVarName valueVarName} dictionary script
static FeatherResult dict_map(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  if (ops->list.length(interp, args) != 3) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict map {keyVarName valueVarName} dictionary script\"", 78);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj varSpec = ops->list.shift(interp, args);
  FeatherObj dict = ops->list.shift(interp, args);
  FeatherObj script = ops->list.shift(interp, args);

  FeatherObj varList = ops->list.from(interp, varSpec);
  if (ops->list.length(interp, varList) != 2) {
    FeatherObj msg = ops->string.intern(interp, "must have exactly two variable names", 36);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
  FeatherObj keyVar = ops->list.at(interp, varList, 0);
  FeatherObj valVar = ops->list.at(interp, varList, 1);

  FeatherObj result = ops->dict.create(interp);
  FeatherObj keys = ops->dict.keys(interp, dict);
  size_t numKeys = ops->list.length(interp, keys);

  for (size_t i = 0; i < numKeys; i++) {
    FeatherObj key = ops->list.at(interp, keys, i);
    FeatherObj val = ops->dict.get(interp, dict, key);

    if (feather_set_var(ops, interp, keyVar, key) != TCL_OK) {
      return TCL_ERROR;
    }
    if (feather_set_var(ops, interp, valVar, val) != TCL_OK) {
      return TCL_ERROR;
    }

    FeatherResult res = feather_script_eval_obj(ops, interp, script, TCL_EVAL_LOCAL);
    if (res == TCL_BREAK) {
      // break returns empty dict
      ops->interp.set_result(interp, ops->dict.create(interp));
      return TCL_OK;
    } else if (res == TCL_CONTINUE) {
      // skip this key-value pair
      continue;
    } else if (res != TCL_OK) {
      return res;
    }

    FeatherObj newVal = ops->interp.get_result(interp);
    result = ops->dict.set(interp, result, key, newVal);
  }

  ops->interp.set_result(interp, result);
  return TCL_OK;
}

// dict update dictVarName key varName ?key varName ...? script
static FeatherResult dict_update(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  // Need: varName, at least one key-varName pair, and script = min 4 args
  if (argc < 4 || (argc - 2) % 2 != 0) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict update dictVarName key varName ?key varName ...? script\"", 86);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj dictVarName = ops->list.shift(interp, args);
  FeatherObj script = ops->list.pop(interp, args);

  // Get current dict
  FeatherObj dict;
  feather_get_var(ops, interp, dictVarName, &dict);
  if (ops->list.is_nil(interp, dict)) {
    dict = ops->dict.create(interp);
  }

  // Collect key-varName pairs
  size_t numPairs = ops->list.length(interp, args) / 2;
  FeatherObj dictKeys[64];
  FeatherObj varNames[64];
  if (numPairs > 64) {
    FeatherObj msg = ops->string.intern(interp, "too many key-variable pairs", 27);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  for (size_t i = 0; i < numPairs; i++) {
    dictKeys[i] = ops->list.shift(interp, args);
    varNames[i] = ops->list.shift(interp, args);

    // Set variable to dict value (or leave unset if key doesn't exist)
    FeatherObj val = ops->dict.get(interp, dict, dictKeys[i]);
    if (!ops->list.is_nil(interp, val)) {
      if (feather_set_var(ops, interp, varNames[i], val) != TCL_OK) {
        return TCL_ERROR;
      }
    }
  }

  // Execute script
  FeatherResult res = feather_script_eval_obj(ops, interp, script, TCL_EVAL_LOCAL);
  FeatherObj scriptResult = ops->interp.get_result(interp);

  // Update dict from variables
  for (size_t i = 0; i < numPairs; i++) {
    FeatherObj val;
    feather_get_var(ops, interp, varNames[i], &val);
    if (ops->list.is_nil(interp, val)) {
      // Variable was unset - remove key from dict
      dict = ops->dict.remove(interp, dict, dictKeys[i]);
    } else {
      // Variable exists - update dict
      dict = ops->dict.set(interp, dict, dictKeys[i], val);
    }
  }

  // Store updated dict
  if (feather_set_var(ops, interp, dictVarName, dict) != TCL_OK) {
    return TCL_ERROR;
  }

  if (res != TCL_OK) {
    return res;
  }

  ops->interp.set_result(interp, scriptResult);
  return TCL_OK;
}

// dict with dictVarName ?key ...? script
static FeatherResult dict_with(const FeatherHostOps *ops, FeatherInterp interp, FeatherObj args) {
  size_t argc = ops->list.length(interp, args);
  if (argc < 2) {
    FeatherObj msg = ops->string.intern(interp,
      "wrong # args: should be \"dict with dictVarName ?key ...? script\"", 64);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }

  FeatherObj dictVarName = ops->list.shift(interp, args);
  FeatherObj script = ops->list.pop(interp, args);

  // Get current dict
  FeatherObj dict;
  feather_get_var(ops, interp, dictVarName, &dict);
  if (ops->list.is_nil(interp, dict)) {
    dict = ops->dict.create(interp);
  }

  // Navigate to nested dict if keys provided
  FeatherObj nestedKeys = ops->list.create(interp);
  while (ops->list.length(interp, args) > 0) {
    FeatherObj key = ops->list.shift(interp, args);
    nestedKeys = ops->list.push(interp, nestedKeys, key);
    FeatherObj nested = ops->dict.get(interp, dict, key);
    if (ops->list.is_nil(interp, nested)) {
      nested = ops->dict.create(interp);
    }
    dict = nested;
  }

  // Get all keys from the target dict
  FeatherObj keys = ops->dict.keys(interp, dict);
  size_t numKeys = ops->list.length(interp, keys);

  // Set variables for each key
  for (size_t i = 0; i < numKeys; i++) {
    FeatherObj key = ops->list.at(interp, keys, i);
    FeatherObj val = ops->dict.get(interp, dict, key);
    if (feather_set_var(ops, interp, key, val) != TCL_OK) {
      return TCL_ERROR;
    }
  }

  // Execute script
  FeatherResult res = feather_script_eval_obj(ops, interp, script, TCL_EVAL_LOCAL);
  FeatherObj scriptResult = ops->interp.get_result(interp);

  // Update dict from variables (only for keys that existed in original dict)
  for (size_t i = 0; i < numKeys; i++) {
    FeatherObj key = ops->list.at(interp, keys, i);
    FeatherObj val;
    feather_get_var(ops, interp, key, &val);
    if (ops->list.is_nil(interp, val)) {
      // Variable was unset - remove key from dict
      dict = ops->dict.remove(interp, dict, key);
    } else {
      // Variable exists - update dict
      dict = ops->dict.set(interp, dict, key, val);
    }
  }

  // If we navigated into nested dict, rebuild from inside out
  size_t numNestedKeys = ops->list.length(interp, nestedKeys);
  if (numNestedKeys > 0) {
    // Need to rebuild the nested structure
    FeatherObj rootDict;
    feather_get_var(ops, interp, dictVarName, &rootDict);
    if (ops->list.is_nil(interp, rootDict)) {
      rootDict = ops->dict.create(interp);
    }

    // Navigate and rebuild
    FeatherObj dicts[65];
    dicts[0] = rootDict;
    for (size_t i = 0; i < numNestedKeys; i++) {
      FeatherObj key = ops->list.at(interp, nestedKeys, i);
      FeatherObj nested = ops->dict.get(interp, dicts[i], key);
      if (ops->list.is_nil(interp, nested)) {
        nested = ops->dict.create(interp);
      }
      dicts[i + 1] = nested;
    }

    // Set innermost dict
    dicts[numNestedKeys] = dict;

    // Rebuild from inside out
    for (size_t i = numNestedKeys; i > 0; i--) {
      FeatherObj key = ops->list.at(interp, nestedKeys, i - 1);
      dicts[i - 1] = ops->dict.set(interp, dicts[i - 1], key, dicts[i]);
    }
    dict = dicts[0];
  }

  // Store updated dict
  if (feather_set_var(ops, interp, dictVarName, dict) != TCL_OK) {
    return TCL_ERROR;
  }

  if (res != TCL_OK) {
    return res;
  }

  ops->interp.set_result(interp, scriptResult);
  return TCL_OK;
}

void feather_register_dict_usage(const FeatherHostOps *ops, FeatherInterp interp) {
  FeatherObj spec = feather_usage_spec(ops, interp);
  FeatherObj subspec;
  FeatherObj e;

  e = feather_usage_about(ops, interp,
    "Manipulate dictionaries",
    "Performs one of several operations on dictionary values or variables "
    "containing dictionary values, depending on the subcommand. Dictionaries "
    "are order-preserving key-value mappings where keys and values can be "
    "arbitrary strings.\n\n"
    "Many subcommands support nested dictionary access by providing multiple "
    "key arguments to navigate through dictionary levels. The maximum nesting "
    "depth is 64 levels.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: append ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<dictVarName>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<key>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?string?...");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "append", subspec);
  e = feather_usage_help(ops, interp, e, "Append strings to a dictionary value");
  e = feather_usage_long_help(ops, interp, e,
    "Appends the given string (or strings) to the value that the given key "
    "maps to in the dictionary value contained in the given variable, writing "
    "the resulting dictionary value back to that variable. Non-existent keys "
    "are treated as if they map to an empty string. The updated dictionary "
    "value is returned.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: create ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "?key value?...");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "create", subspec);
  e = feather_usage_help(ops, interp, e, "Create a new dictionary");
  e = feather_usage_long_help(ops, interp, e,
    "Returns a new dictionary that contains each of the key/value mappings "
    "listed as arguments (keys and values alternating, with each key being "
    "followed by its associated value).");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: exists ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<dictionary>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<key>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?key?...");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "exists", subspec);
  e = feather_usage_help(ops, interp, e, "Check if a key exists in a dictionary");
  e = feather_usage_long_help(ops, interp, e,
    "Returns a boolean value indicating whether the given key (or path of "
    "keys through a set of nested dictionaries) exists in the given dictionary "
    "value. This returns a true value exactly when dict get on that path will "
    "succeed.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: filter ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<dictionary>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<filterType>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?arg?...");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "filter", subspec);
  e = feather_usage_help(ops, interp, e, "Filter dictionary by key, value, or script");
  e = feather_usage_long_help(ops, interp, e,
    "Takes a dictionary value and returns a new dictionary that contains just "
    "those key/value pairs that match the specified filter type. Supported "
    "filter types are:\n\n"
    "dict filter dictionary key ?globPattern ...?\n"
    "    Matches key/value pairs whose keys match any of the given patterns "
    "(in the style of string match).\n\n"
    "dict filter dictionary value ?globPattern ...?\n"
    "    Matches key/value pairs whose values match any of the given patterns.\n\n"
    "dict filter dictionary script {keyVar valueVar} script\n"
    "    Tests for matching by assigning the key to keyVar and value to "
    "valueVar, then evaluating the script which should return a boolean. "
    "break stops filtering and returns results so far; continue skips the "
    "current pair.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: for ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "{keyVar valueVar}");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<dictionary>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<body>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "for", subspec);
  e = feather_usage_help(ops, interp, e, "Iterate over dictionary key-value pairs");
  e = feather_usage_long_help(ops, interp, e,
    "Iterates over the key-value pairs in the dictionary. The first argument "
    "is a two-element list of variable names (for the key and value "
    "respectively), the second is the dictionary value to iterate, and the "
    "third is a script to be evaluated for each mapping with the key and value "
    "variables set appropriately.\n\n"
    "The result is an empty string. If the body generates a break result, "
    "iteration stops immediately. A continue result is treated like a normal "
    "return. The order of iteration is the order in which keys were inserted "
    "into the dictionary.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: get ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<dictionary>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?key?...");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "get", subspec);
  e = feather_usage_help(ops, interp, e, "Get value for a key from dictionary");
  e = feather_usage_long_help(ops, interp, e,
    "Given a dictionary value and a key, retrieves the value for that key. "
    "When several keys are supplied, this facilitates lookups in nested "
    "dictionaries: the result of dict get $dict foo bar is equivalent to "
    "dict get [dict get $dict foo] bar.\n\n"
    "If no keys are provided, dict get returns a list containing pairs of "
    "elements in a manner similar to array get. That is, the first element of "
    "each pair is the key and the second is the value.\n\n"
    "It is an error to attempt to retrieve a value for a key that is not "
    "present in the dictionary.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: getdef / getwithdefault ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<dictionary>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?key?...");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<key>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<default>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "getdef", subspec);
  e = feather_usage_help(ops, interp, e, "Get value with default if key missing");
  e = feather_usage_long_help(ops, interp, e,
    "Behaves the same as dict get (with at least one key argument), returning "
    "the value that the key path maps to in the dictionary, except that "
    "instead of producing an error because the key (or one of the keys on the "
    "key path) is absent, it returns the default argument instead.\n\n"
    "Note that there must always be at least one key provided. "
    "dict getwithdefault is an alias for dict getdef.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: incr ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<dictVarName>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<key>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?increment?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "incr", subspec);
  e = feather_usage_help(ops, interp, e, "Increment a dictionary value");
  e = feather_usage_long_help(ops, interp, e,
    "Adds the given increment value (an integer that defaults to 1 if not "
    "specified) to the value that the given key maps to in the dictionary "
    "value contained in the given variable, writing the resulting dictionary "
    "value back to that variable. Non-existent keys are treated as if they "
    "map to 0. It is an error to increment a value for an existing key if "
    "that value is not an integer. The updated dictionary value is returned.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: info ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<dictionary>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "info", subspec);
  e = feather_usage_help(ops, interp, e, "Get information about a dictionary");
  e = feather_usage_long_help(ops, interp, e,
    "Returns information (intended for display to people) about the given "
    "dictionary. In feather, this returns a string of the form \"N entries in "
    "table\" where N is the number of key-value pairs.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: keys ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<dictionary>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?globPattern?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "keys", subspec);
  e = feather_usage_help(ops, interp, e, "Get list of keys from dictionary");
  e = feather_usage_long_help(ops, interp, e,
    "Returns a list of all keys in the given dictionary value. If a pattern "
    "is supplied, only those keys that match it (according to the rules of "
    "string match) will be returned. The returned keys will be in the order "
    "that they were inserted into the dictionary.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: lappend ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<dictVarName>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<key>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?value?...");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "lappend", subspec);
  e = feather_usage_help(ops, interp, e, "Append list elements to a dictionary value");
  e = feather_usage_long_help(ops, interp, e,
    "Appends the given items to the list value that the given key maps to in "
    "the dictionary value contained in the given variable, writing the "
    "resulting dictionary value back to that variable. Non-existent keys are "
    "treated as if they map to an empty list, and it is legal for there to be "
    "no items to append. It is an error for the value that the key maps to to "
    "not be representable as a list. The updated dictionary value is returned.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: map ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "{keyVar valueVar}");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<dictionary>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<body>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "map", subspec);
  e = feather_usage_help(ops, interp, e, "Transform dictionary values with a script");
  e = feather_usage_long_help(ops, interp, e,
    "Applies a transformation to each element of a dictionary, returning a "
    "new dictionary. The first argument is a two-element list of variable "
    "names (for key and value), the second is the dictionary to iterate, and "
    "the third is a script evaluated for each mapping. The result of each "
    "script evaluation becomes the new value for that key.\n\n"
    "If the body generates a break result, the command returns an empty "
    "dictionary immediately. A continue result skips the current key-value "
    "pair (it is not included in the result). The order of iteration is the "
    "order in which keys were inserted.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: merge ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "?dictionary?...");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "merge", subspec);
  e = feather_usage_help(ops, interp, e, "Merge multiple dictionaries into one");
  e = feather_usage_long_help(ops, interp, e,
    "Returns a dictionary that contains the contents of each of the dictionary "
    "arguments. Where two or more dictionaries contain a mapping for the same "
    "key, the resulting dictionary maps that key to the value according to the "
    "last dictionary on the command line containing a mapping for that key.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: remove ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<dictionary>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?key?...");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "remove", subspec);
  e = feather_usage_help(ops, interp, e, "Remove keys from a dictionary");
  e = feather_usage_long_help(ops, interp, e,
    "Returns a new dictionary that is a copy of the old one passed in as the "
    "first argument except without mappings for each of the keys listed. It "
    "is legal for there to be no keys to remove, and it is also legal for any "
    "of the keys to be removed to not be present in the input dictionary.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: replace ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<dictionary>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?key value?...");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "replace", subspec);
  e = feather_usage_help(ops, interp, e, "Replace or add key-value pairs in dictionary");
  e = feather_usage_long_help(ops, interp, e,
    "Returns a new dictionary that is a copy of the old one passed in as the "
    "first argument except with some values different or some extra key/value "
    "pairs added. It is legal for this command to be called with no key/value "
    "pairs, but illegal for this command to be called with a key but no value.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: set ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<dictVarName>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<key>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?key?...");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<value>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "set", subspec);
  e = feather_usage_help(ops, interp, e, "Set a value in a dictionary variable");
  e = feather_usage_long_help(ops, interp, e,
    "Takes the name of a variable containing a dictionary value and places an "
    "updated dictionary value in that variable containing a mapping from the "
    "given key to the given value. When multiple keys are present, this "
    "operation creates or updates a chain of nested dictionaries. The updated "
    "dictionary value is returned.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: size ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<dictionary>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "size", subspec);
  e = feather_usage_help(ops, interp, e, "Get number of entries in dictionary");
  e = feather_usage_long_help(ops, interp, e,
    "Returns the number of key/value mappings in the given dictionary value.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: unset ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<dictVarName>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<key>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?key?...");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "unset", subspec);
  e = feather_usage_help(ops, interp, e, "Remove a key from a dictionary variable");
  e = feather_usage_long_help(ops, interp, e,
    "Takes the name of a variable containing a dictionary value and places an "
    "updated dictionary value in that variable that does not contain a mapping "
    "for the given key. Where multiple keys are present, this describes a path "
    "through nested dictionaries to the mapping to remove. At least one key "
    "must be specified, but the last key on the key-path need not exist. All "
    "other components on the path must exist. The updated dictionary value is "
    "returned.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: update ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<dictVarName>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<key>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<varName>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?key varName?...");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<body>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "update", subspec);
  e = feather_usage_help(ops, interp, e, "Update dictionary values using variables");
  e = feather_usage_long_help(ops, interp, e,
    "Executes the script in body with the value for each key (as found by "
    "reading the dictionary value in dictVarName) mapped to the variable "
    "varName. There may be multiple key/varName pairs. If a key does not have "
    "a mapping, the corresponding varName is not created.\n\n"
    "When body terminates, any changes made to the varNames are reflected back "
    "to the dictionary within dictVarName. If a variable is unset during body, "
    "the corresponding key is removed from the dictionary. The result of dict "
    "update is the result of the evaluation of body.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: values ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<dictionary>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?globPattern?");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "values", subspec);
  e = feather_usage_help(ops, interp, e, "Get list of values from dictionary");
  e = feather_usage_long_help(ops, interp, e,
    "Returns a list of all values in the given dictionary value. If a pattern "
    "is supplied, only those values that match it (according to the rules of "
    "string match) will be returned. The returned values will be in the order "
    "of the keys associated with those values were inserted into the "
    "dictionary.");
  spec = feather_usage_add(ops, interp, spec, e);

  // --- Subcommand: with ---
  subspec = feather_usage_spec(ops, interp);
  e = feather_usage_arg(ops, interp, "<dictVarName>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "?key?...");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_arg(ops, interp, "<body>");
  subspec = feather_usage_add(ops, interp, subspec, e);
  e = feather_usage_cmd(ops, interp, "with", subspec);
  e = feather_usage_help(ops, interp, e, "Execute script with dictionary keys as variables");
  e = feather_usage_long_help(ops, interp, e,
    "Executes the script in body with the value for each key in dictVarName "
    "mapped to a variable with the same name as the key. Where one or more "
    "keys are provided, these indicate a chain of nested dictionaries, with "
    "the innermost dictionary being the one opened out for execution.\n\n"
    "After body executes, any changes made to the variables are reflected back "
    "to the dictionary. If a variable is unset, the corresponding key is "
    "removed. New variables created during body are NOT added as new keys "
    "(only existing keys are tracked). The result is the result of body.");
  spec = feather_usage_add(ops, interp, spec, e);

  // Examples
  e = feather_usage_example(ops, interp,
    "dict create .txt \"Text File\" .tcl \"Tcl Script\"",
    "Create a dictionary to map file extensions to descriptions",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "dict set employeeInfo 12345 name \"Joe Schmoe\"",
    "Set a nested dictionary value (creates nested structure if needed)",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "dict for {id info} $employees { puts \"$id: [dict get $info name]\" }",
    "Iterate over all key-value pairs in the dictionary",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_example(ops, interp,
    "dict filter $mydict key a* b*",
    "Filter dictionary to only keys matching patterns \"a*\" or \"b*\"",
    NULL);
  spec = feather_usage_add(ops, interp, spec, e);

  e = feather_usage_section(ops, interp, "See Also",
    "list, string match");
  spec = feather_usage_add(ops, interp, spec, e);

  feather_usage_register(ops, interp, "dict", spec);
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
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "filter")) {
    return dict_filter(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "map")) {
    return dict_map(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "update")) {
    return dict_update(ops, interp, args);
  } else if (feather_obj_eq_literal(ops, interp, subcmd, "with")) {
    return dict_with(ops, interp, args);
  } else {
    FeatherObj msg = ops->string.intern(interp, "unknown or ambiguous subcommand \"", 33);
    msg = ops->string.concat(interp, msg, subcmd);
    FeatherObj suffix = ops->string.intern(interp,
      "\": must be append, create, exists, filter, for, get, getdef, getwithdefault, incr, info, keys, lappend, map, merge, remove, replace, set, size, unset, update, values, or with", 174);
    msg = ops->string.concat(interp, msg, suffix);
    ops->interp.set_result(interp, msg);
    return TCL_ERROR;
  }
}
