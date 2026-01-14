# Feather API Comparison: C, Go, JS

This document compares the three public APIs for the Feather TCL interpreter.

## Legend

| Symbol | Meaning |
|--------|---------|
| ✓ | Implemented |
| ✗ | Missing |
| ~ | Partial/Different |

---

## Lifecycle

| Operation | C API | Go API | JS API |
|-----------|-------|--------|--------|
| Create interpreter | `FeatherNew()` → `FeatherInterp` | `feather.New()` → `*Interp` | `feather.create()` → `number` |
| Destroy interpreter | `FeatherClose(interp)` | `interp.Close()` | `feather.destroy(id)` |

**Status: ✓ Consistent**

---

## Evaluation

| Operation | C API | Go API | JS API |
|-----------|-------|--------|--------|
| Evaluate script | `FeatherEval(interp, script, len, &result)` | `interp.Eval(script)` → `(*Obj, error)` | `feather.eval(id, script)` → `string` |
| Direct command call | `FeatherCall(interp, argc, argv, &result)` | `interp.Call(cmd, args...)` → `(*Obj, error)` | ✗ **Missing** |
| Get result | Returned via pointer | Returned from method | `feather.getResult(id)` |
| Parse check | `FeatherParse(interp, script, len)` | `interp.Parse(script)` → `ParseResult` | `feather.parse(id, script)` |
| Parse with info | `FeatherParseInfo(...)` | ✗ **Missing** | ~ Returns `{status, result, errorMessage}` |

**Issues:**
- JS lacks `Call` for safe argument passing without TCL parsing
- Go lacks `ParseInfo` with detailed position info

---

## Object Creation

| Type | C API | Go API | JS API |
|------|-------|--------|--------|
| String | `FeatherString(interp, s, len)` | `interp.String(s)` | Internal only |
| Int | `FeatherInt(interp, val)` | `interp.Int(val)` | Internal only |
| Double | `FeatherDouble(interp, val)` | `interp.Double(val)` | Internal only |
| Bool | ✗ **Missing** | `interp.Bool(val)` | Internal only |
| List | `FeatherList(interp, argc, argv)` | `interp.List(items...)` | Internal only |
| Dict | `FeatherDict(interp)` (empty only) | `interp.Dict()`, `interp.DictKV(...)`, `interp.DictFrom(map)` | Internal only |
| List from slice | ✗ **Missing** | `interp.ListFrom(slice)` | ✗ |

**Issues:**
- C lacks `Bool` constructor
- C `FeatherDict` only creates empty dicts; Go has richer constructors
- JS has no public object creation API (all internal)

---

## Type Conversion / Getters

| Operation | C API | Go API | JS API |
|-----------|-------|--------|--------|
| To int | `FeatherAsInt(interp, obj, def)` | `obj.Int()` → `(int64, error)` | Internal `getInt()` |
| To double | `FeatherAsDouble(interp, obj, def)` | `obj.Double()` → `(float64, error)` | Internal |
| To bool | `FeatherAsBool(interp, obj, def)` | `obj.Bool()` → `(bool, error)` | Internal |
| To string | via `FeatherCopy` | `obj.String()` | `interp.getString(h)` |
| To list | ✗ **Missing** | `obj.List()` → `([]*Obj, error)` | `interp.getList(h)` |
| To dict | ✗ **Missing** | `obj.Dict()` → `(map[string]*Obj, error)` | `interp.getDict(h)` |

**Issues:**
- C uses default values on failure; Go returns errors (different semantics)
- C lacks list/dict getters on objects

---

## String Operations

| Operation | C API | Go API | JS API |
|-----------|-------|--------|--------|
| Length | `FeatherLen(interp, obj)` | `len(obj.String())` | `getString(h).length` |
| Byte at index | `FeatherByteAt(interp, obj, idx)` | Via string indexing | Via string indexing |
| Copy to buffer | `FeatherCopy(interp, obj, buf, len)` | N/A (strings are values) | N/A |
| Equality | `FeatherEq(interp, a, b)` | `a.String() == b.String()` | `===` |
| Compare | `FeatherCmp(interp, a, b)` | `strings.Compare(...)` | `localeCompare` |

**Status: ~ Different paradigms** (C needs buffer copies; Go/JS have value strings)

---

## List Operations

| Operation | C API | Go API | JS API |
|-----------|-------|--------|--------|
| Length | `FeatherListLen(interp, list)` | `len(obj.List())` | `getList(h).items.length` |
| Get element | `FeatherListAt(interp, list, idx)` | `list[idx]` (after `obj.List()`) | `items[idx]` |
| Append | `FeatherListPush(interp, list, item)` | ✗ **Missing** | ✗ **Missing** |

**Issues:**
- Go and JS lack push/append for lists (must rebuild)

---

## Dict Operations

| Operation | C API | Go API | JS API |
|-----------|-------|--------|--------|
| Length | `FeatherDictLen(interp, dict)` | `len(dict)` (after `obj.Dict()`) | `entries.length` |
| Get value | `FeatherDictGet(interp, dict, key)` | `dict[key]` | `entries[i][1]` |
| Set value | `FeatherDictSet(interp, dict, k, v)` | ✗ **Missing (immutable)** | ✗ |
| Has key | `FeatherDictHas(interp, dict, key)` | `_, ok := dict[key]` | Loop search |
| Get keys | `FeatherDictKeys(interp, dict)` | Iterate map | Iterate entries |

**Issues:**
- Go/JS treat dicts as immutable; C has mutation API

---

## Variables

| Operation | C API | Go API | JS API |
|-----------|-------|--------|--------|
| Get variable | `FeatherGetVar(interp, name)` | `interp.Var(name)` | ✗ **Missing** |
| Set variable | `FeatherSetVar(interp, name, val)` | `interp.SetVar(name, val)` | ✗ **Missing** |
| Get multiple | ✗ | `interp.GetVars(names...)` | ✗ |
| Set multiple | ✗ | `interp.SetVars(map)` | ✗ |

**Issues:**
- JS has no variable access API (must use `eval("set varname")`)

---

## Command Registration

| Operation | C API | Go API | JS API |
|-----------|-------|--------|--------|
| Register command | `FeatherRegister(interp, name, fn, data)` | `interp.Register(name, fn)` | `feather.register(id, name, fn)` |
| Register (low-level) | Same as above | `interp.RegisterCommand(name, fn)` | Same as above |
| Unregister | ✗ **Missing** | `interp.UnregisterCommand(name)` | ✗ **Missing** |
| Unknown handler | ✗ **Missing** | `interp.SetUnknownHandler(fn)` | ✗ **Missing** |

**Issues:**
- C/JS lack unregister capability
- C/JS lack unknown command handler

---

## Foreign Types

| Operation | C API | Go API | JS API |
|-----------|-------|--------|--------|
| Register type | `FeatherRegisterForeign(...)` | `feather.RegisterType[T](interp, name, def)` | `feather.registerType(id, name, def)` |
| Register method | `FeatherRegisterForeignMethod(...)` | Via `TypeDef.Methods` map | Via `typeDef.methods` |
| Create instance | Via constructor callback | Via `TypeDef.New` | `feather.createForeign(id, type, val)` |
| Destroy handler | Via callback | Via `TypeDef.Destroy` | Via `typeDef.destroy` |
| String representation | Via callback | Via `TypeDef.String` | ~ Via `stringRep` param |

**Status: ~ All have foreign type support, but different interfaces**

---

## Parsing

| Status | C API | Go API | JS API |
|--------|-------|--------|--------|
| OK | `FEATHER_PARSE_OK` | `feather.ParseOK` | `TCL_PARSE_OK` |
| Incomplete | `FEATHER_PARSE_INCOMPLETE` | `feather.ParseIncomplete` | `TCL_PARSE_INCOMPLETE` |
| Error | `FEATHER_PARSE_ERROR` | `feather.ParseError` | `TCL_PARSE_ERROR` |

**Status: ✓ Consistent (different naming conventions)**

---

## Result Codes

| Code | C API | Go API | JS API |
|------|-------|--------|--------|
| OK | `FEATHER_OK` | `feather.ResultOK` | `TCL_OK` |
| Error | `FEATHER_ERROR` | `feather.ResultError` | `TCL_ERROR` |
| Return | ✗ | Internal | `TCL_RETURN` |
| Break | ✗ | Internal | `TCL_BREAK` |
| Continue | ✗ | Internal | `TCL_CONTINUE` |

**Issues:**
- C only exposes OK/ERROR; JS exposes all TCL codes

---

## Summary of Gaps

### C API Needs:
1. `FeatherBool()` constructor
2. `FeatherAsList()` / `FeatherAsDict()` getters
3. `FeatherUnregister()` for command removal
4. `FeatherSetUnknown()` for unknown handler
5. Rich dict constructors (or accept key-value array)

### Go API Needs:
1. `ParseInfo()` with position details
2. List mutation helpers (`Push`, `Concat`)
3. (Dict mutation is intentionally immutable)

### JS API Needs:
1. `call(id, cmd, args)` - Direct command invocation
2. `getVar(id, name)` / `setVar(id, name, val)` - Variable access
3. `unregister(id, name)` - Command removal
4. `setUnknownHandler(id, fn)` - Unknown command hook
5. Public object creation (`string`, `int`, `list`, etc.)

---

## Recommended Priority

1. **JS: Add `call()`** - Most impactful for safe arg passing
2. **JS: Add variable access** - Essential for integration
3. **C: Add `FeatherBool()`** - Simple parity fix
4. **C: Add list/dict getters** - Important for data exchange
5. **Go: Add `ParseInfo()`** - Useful for tooling
