/**
 * feather.js - JavaScript host for feather WASM interpreter
 *
 * Works in both Node.js and browsers using ES modules.
 * Provides host function implementations as direct WASM imports.
 */

const TCL_OK = 0;
const TCL_ERROR = 1;
const TCL_RETURN = 2;
const TCL_BREAK = 3;
const TCL_CONTINUE = 4;

// Parse status constants (matching FeatherParseStatus enum in feather.h)
const TCL_PARSE_OK = 0;
const TCL_PARSE_INCOMPLETE = 1;
const TCL_PARSE_ERROR = 2;
const TCL_PARSE_DONE = 3;

const TCL_CMD_NONE = 0;
const TCL_CMD_BUILTIN = 1;
const TCL_CMD_PROC = 2;

const DEFAULT_RECURSION_LIMIT = 200;

class FeatherInterp {
  constructor(id) {
    this.id = id;
    
    // Scratch arena - reset after each top-level eval
    this.scratch = {
      objects: new Map(),
      nextHandle: 1,
    };
    this.evalDepth = 0;  // Track nested eval depth
    
    this.result = 0;
    // Global namespace - shared between namespace storage and frame 0
    const globalNS = { vars: new Map(), children: new Map(), exports: [], commands: new Map() };
    this.namespaces = new Map([['', globalNS]]);
    // Frame 0's vars IS the global namespace's vars (unified storage)
    this.frames = [{ vars: globalNS.vars, cmd: 0, args: 0, ns: '::' }];
    this.activeLevel = 0;
    this.procs = new Map();
    this.builtins = new Map();
    this.hostCommands = new Map();
    this.traces = { variable: new Map(), command: new Map() };
    this.returnOptions = new Map();
    this.scriptPath = '';
    this.foreignTypes = new Map();
    this.foreignInstances = new Map(); // handle name -> { typeName, value, objHandle }
    this.recursionLimit = DEFAULT_RECURSION_LIMIT;
    // Injected by createFeather() - calls C's feather_list_parse via WASM
    this._parseListFromC = null;
  }

  store(obj) {
    const handle = this.scratch.nextHandle++;
    this.scratch.objects.set(handle, obj);
    return handle;
  }

  invalidateStringCache(obj) {
    if (obj && obj.type === 'list') {
      delete obj.cachedString;
    }
  }

  get(handle) {
    return this.scratch.objects.get(handle);
  }

  /**
   * Reset the scratch arena, reclaiming all handle memory.
   * Only call at top-level eval boundaries (evalDepth === 0).
   * Invalidates all handles from previous allocations.
   */
  resetScratch() {
    this.scratch = { objects: new Map(), nextHandle: 1 };
  }

  /**
   * Materialize a handle into a persistent value (deep copy).
   * Handles are only valid during a single eval. To store values
   * persistently (in procs, namespaces, traces), materialize them.
   */
  materialize(handle) {
    if (handle === 0) return null;
    const obj = this.get(handle);
    if (!obj) return null;
    
    if (obj.type === 'string') return { type: 'string', value: obj.value };
    if (obj.type === 'int') return { type: 'int', value: obj.value };
    if (obj.type === 'double') return { type: 'double', value: obj.value };
    if (obj.type === 'list') {
      return { type: 'list', items: obj.items.map(h => this.materialize(h)) };
    }
    if (obj.type === 'dict') {
      return { 
        type: 'dict', 
        entries: obj.entries.map(([k, v]) => [this.materialize(k), this.materialize(v)])
      };
    }
    if (obj.type === 'foreign') {
      // Foreign objects can't be fully materialized; store reference info
      return { type: 'foreign', typeName: obj.typeName, stringRep: obj.stringRep };
    }
    // Fallback
    return { type: 'string', value: this.getString(handle) };
  }

  /**
   * Wrap a materialized value into a fresh scratch handle.
   * When retrieving from persistent storage, wrap values to get
   * handles that C code can use during this eval.
   */
  wrap(value) {
    if (value === null || value === undefined) return 0;
    
    if (value.type === 'list') {
      const items = value.items.map(item => this.wrap(item));
      return this.store({ type: 'list', items });
    }
    if (value.type === 'dict') {
      const entries = value.entries.map(([k, v]) => [this.wrap(k), this.wrap(v)]);
      return this.store({ type: 'dict', entries });
    }
    // Primitives: string, int, double, foreign
    return this.store({ ...value });
  }

  getString(handle) {
    if (handle === 0) return '';
    const obj = this.get(handle);
    if (!obj) return '';
    if (typeof obj === 'string') return obj;
    if (obj.type === 'string') return obj.value;
    if (obj.type === 'int') return String(obj.value);
    if (obj.type === 'double') {
      // Format special values to match TCL's format
      if (Number.isNaN(obj.value)) return 'NaN';
      if (obj.value === Infinity) return 'Inf';
      if (obj.value === -Infinity) return '-Inf';
      // TCL requires floats to always have a decimal point
      const s = String(obj.value);
      if (s.includes('.') || s.includes('e') || s.includes('E')) return s;
      return s + '.0';
    }
    if (obj.type === 'list') return obj.items.map(h => this.quoteListElement(this.getString(h))).join(' ');
    if (obj.type === 'dict') {
      const parts = [];
      for (const [k, v] of obj.entries) {
        parts.push(this.quoteListElement(this.getString(k)), this.quoteListElement(this.getString(v)));
      }
      return parts.join(' ');
    }
    if (obj.type === 'foreign') return obj.stringRep || `<${obj.typeName}:${handle}>`;
    return String(obj);
  }

  quoteListElement(str) {
    if (str === '') return '{}';
    if (/[\s{}\\"]/.test(str) || str.includes('\n')) {
      return '{' + str + '}';
    }
    return str;
  }

  // Check if an object is foreign (directly or by handle name)
  isForeign(handle) {
    const obj = this.get(handle);
    if (obj?.type === 'foreign') return true;
    // Check if string value is a foreign handle name
    const strVal = this.getString(handle);
    return this.foreignInstances.has(strVal);
  }

  // Get foreign type name (checks direct type and handle name)
  getForeignTypeName(handle) {
    const obj = this.get(handle);
    if (obj?.type === 'foreign') return obj.typeName;
    // Check if string value is a foreign handle name
    const strVal = this.getString(handle);
    const instance = this.foreignInstances.get(strVal);
    return instance?.typeName || null;
  }

  // Get foreign instance info by handle
  getForeignInstance(handle) {
    const obj = this.get(handle);
    if (obj?.type === 'foreign') {
      // Find instance by searching (could optimize with reverse map)
      for (const [name, inst] of this.foreignInstances) {
        if (inst.objHandle === handle) return inst;
      }
      return null;
    }
    // Check if string value is a foreign handle name
    const strVal = this.getString(handle);
    return this.foreignInstances.get(strVal) || null;
  }

  getList(handle) {
    if (handle === 0) return { items: [] };
    const obj = this.get(handle);
    if (!obj) return { items: [] };
    if (obj.type === 'list') return { items: obj.items };

    // Use C's feather_list_parse
    const str = this.getString(handle);
    const listHandle = this._parseListFromC(str);
    if (listHandle === 0) {
      // Error - get message from result
      const errObj = this.get(this.result);
      return { error: errObj ? errObj.value : 'parse error' };
    }
    const listObj = this.get(listHandle);
    if (listObj?.type === 'list') {
      // Cache the list type on the original object for shimmering
      obj.type = 'list';
      obj.items = listObj.items;
      return { items: obj.items };
    }
    return { items: [] };
  }

  // getDict shimmers string/list to dict representation
  // Returns { entries: [[keyHandle, valHandle], ...], order: [key1, key2, ...] } or null on error
  getDict(handle) {
    if (handle === 0) return null;
    const obj = this.get(handle);
    if (!obj) return null;

    // Already a dict - return cached
    if (obj.type === 'dict') {
      return {
        entries: obj.entries,
        order: obj.entries.map(([k]) => this.getString(k))
      };
    }

    // Shimmer: get as list first, then convert to dict
    const listResult = this.getList(handle);
    if (listResult.error) {
      return null; // parse error
    }
    const items = listResult.items;
    if (items.length % 2 !== 0) {
      return null; // odd number of elements
    }

    // Build dict entries preserving order
    const entries = [];
    const order = [];
    const seen = new Set();
    for (let i = 0; i < items.length; i += 2) {
      const keyStr = this.getString(items[i]);
      if (!seen.has(keyStr)) {
        seen.add(keyStr);
        order.push(keyStr);
      }
      // Find existing entry to update, or add new
      let found = false;
      for (let j = 0; j < entries.length; j++) {
        if (this.getString(entries[j][0]) === keyStr) {
          entries[j][1] = items[i + 1];
          found = true;
          break;
        }
      }
      if (!found) {
        entries.push([items[i], items[i + 1]]);
      }
    }

    // Cache the parsed dict representation on the object
    obj.type = 'dict';
    obj.entries = entries;

    return { entries, order };
  }

  currentFrame() {
    return this.frames[this.activeLevel];
  }

  getNamespace(path) {
    const normalized = path.replace(/^::/, '');
    return this.namespaces.get(normalized);
  }

  ensureNamespace(path) {
    const normalized = path.replace(/^::/, '');
    if (this.namespaces.has(normalized)) return this.namespaces.get(normalized);

    const parts = normalized.split('::').filter(p => p);
    let current = '';
    for (const part of parts) {
      const parent = this.namespaces.get(current);
      current = current ? `${current}::${part}` : part;
      if (!this.namespaces.has(current)) {
        const ns = { vars: new Map(), children: new Map(), exports: [], commands: new Map() };
        this.namespaces.set(current, ns);
        if (parent) parent.children.set(part, current);
      }
    }
    return this.namespaces.get(normalized);
  }
}

async function createFeather(wasmSource) {
  const interpreters = new Map();
  let nextInterpId = 1;
  let wasmMemory;
  let wasmInstance;

  // Helper to fire variable traces
  const fireVarTraces = (interp, varName, op) => {
    const traces = interp.traces.variable.get(varName);
    if (!traces || traces.length === 0) return;

    for (const trace of traces) {
      // Check if this trace matches the operation
      const ops = trace.ops.split(/\s+/);
      if (!ops.includes(op)) continue;

      // Build the command: script name1 name2 op
      // trace.script is now stored as string directly
      const cmd = `${trace.script} ${varName} {} ${op}`;
      const [ptr, len] = writeString(cmd);
      wasmInstance.exports.feather_script_eval(0, interp.id, ptr, len, 0);
      wasmInstance.exports.free(ptr);
    }
  };

  // Helper to convert fully qualified name to display name (strip :: for global commands)
  const displayName = (name) => {
    if (name.length > 2 && name.startsWith('::')) {
      const rest = name.slice(2);
      if (!rest.includes('::')) {
        return rest;
      }
    }
    return name;
  };

  // Helper to fire command traces
  const fireCmdTraces = (interp, oldName, newName, op) => {
    // Look up traces using the qualified name (traces are stored with :: prefix)
    const qualifiedOld = oldName.startsWith('::') ? oldName : '::' + oldName;
    const traces = interp.traces.command.get(qualifiedOld);
    if (!traces || traces.length === 0) return;

    for (const trace of traces) {
      // Check if this trace matches the operation
      const ops = trace.ops.split(/\s+/);
      if (!ops.includes(op)) continue;

      // Build the command: script oldName newName op
      // Use display names (strip :: for global namespace commands)
      // trace.script is now stored as string directly
      const displayOld = displayName(oldName);
      const displayNew = displayName(newName);
      // Empty strings must be properly quoted with {}
      const quotedNew = displayNew === '' ? '{}' : displayNew;
      const cmd = `${trace.script} ${displayOld} ${quotedNew} ${op}`;
      const [ptr, len] = writeString(cmd);
      wasmInstance.exports.feather_script_eval(0, interp.id, ptr, len, 0);
      wasmInstance.exports.free(ptr);
    }
  };

  const readString = (ptr, len) => {
    const bytes = new Uint8Array(wasmMemory.buffer, ptr, len);
    return new TextDecoder().decode(bytes);
  };

  const writeString = (str) => {
    const bytes = new TextEncoder().encode(str);
    const ptr = wasmInstance.exports.alloc(bytes.length + 1);
    new Uint8Array(wasmMemory.buffer, ptr, bytes.length).set(bytes);
    new Uint8Array(wasmMemory.buffer)[ptr + bytes.length] = 0;
    return [ptr, bytes.length];
  };

  const writeI32 = (ptr, value) => {
    new DataView(wasmMemory.buffer).setInt32(ptr, value, true);
  };

  const readI32 = (ptr) => {
    return new DataView(wasmMemory.buffer).getInt32(ptr, true);
  };

  const writeI64 = (ptr, value) => {
    new DataView(wasmMemory.buffer).setBigInt64(ptr, BigInt(value), true);
  };

  const readI64 = (ptr) => {
    return Number(new DataView(wasmMemory.buffer).getBigInt64(ptr, true));
  };

  const writeF64 = (ptr, value) => {
    new DataView(wasmMemory.buffer).setFloat64(ptr, value, true);
  };

  // Host function implementations - provided as WASM imports
  const hostImports = {
    // Frame operations
    // NOTE: Frame cmd/args store raw handles. This is safe because:
    // 1. Frames are always popped before eval returns
    // 2. Arena reset only happens at top-level eval completion
    // 3. If we ever support frame introspection across evals, revisit this
    feather_host_frame_push: (interpId, cmd, args) => {
      const interp = interpreters.get(interpId);
      // Check recursion limit
      if (interp.frames.length >= interp.recursionLimit) {
        const msg = interp.store({ type: 'string', value: 'too many nested evaluations (infinite loop?)' });
        interp.result = msg;
        return TCL_ERROR;
      }
      const parentNs = interp.frames[interp.frames.length - 1].ns;
      // New frames get their own vars Map (NOT shared with namespace)
      interp.frames.push({ vars: new Map(), cmd, args, ns: parentNs });
      interp.activeLevel = interp.frames.length - 1;
      return TCL_OK;
    },
    feather_host_frame_pop: (interpId) => {
      const interp = interpreters.get(interpId);
      if (interp.frames.length > 1) {
        interp.frames.pop();
        interp.activeLevel = Math.min(interp.activeLevel, interp.frames.length - 1);
      }
      return TCL_OK;
    },
    feather_host_frame_level: (interpId) => {
      return interpreters.get(interpId).activeLevel;
    },
    feather_host_frame_set_active: (interpId, level) => {
      const interp = interpreters.get(interpId);
      if (level >= interp.frames.length) return TCL_ERROR;
      interp.activeLevel = level;
      return TCL_OK;
    },
    feather_host_frame_size: (interpId) => {
      return interpreters.get(interpId).frames.length;
    },
    feather_host_frame_info: (interpId, level, cmdPtr, argsPtr, nsPtr) => {
      const interp = interpreters.get(interpId);
      if (level >= interp.frames.length) return TCL_ERROR;
      const frame = interp.frames[level];
      writeI32(cmdPtr, frame.cmd);
      writeI32(argsPtr, frame.args);
      const nsHandle = interp.store({ type: 'string', value: frame.ns });
      writeI32(nsPtr, nsHandle);
      return TCL_OK;
    },
    feather_host_frame_set_namespace: (interpId, ns) => {
      const interp = interpreters.get(interpId);
      interp.currentFrame().ns = interp.getString(ns);
      return TCL_OK;
    },
    feather_host_frame_get_namespace: (interpId) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'string', value: interp.currentFrame().ns });
    },

    // Variable operations
    feather_host_var_get: (interpId, name) => {
      const interp = interpreters.get(interpId);
      const varName = interp.getString(name);
      const frame = interp.currentFrame();
      const entry = frame.vars.get(varName);
      if (!entry) return 0;
      let materialized;
      if (entry.link) {
        const targetFrame = interp.frames[entry.link.level];
        const targetEntry = targetFrame?.vars.get(entry.link.name);
        if (!targetEntry) return 0;
        materialized = typeof targetEntry === 'object' && 'value' in targetEntry ? targetEntry.value : targetEntry;
      } else if (entry.nsLink) {
        const ns = interp.getNamespace(entry.nsLink.ns);
        const nsEntry = ns?.vars.get(entry.nsLink.name);
        if (!nsEntry) return 0;
        materialized = typeof nsEntry === 'object' && 'value' in nsEntry ? nsEntry.value : nsEntry;
      } else {
        materialized = entry.value;
      }
      if (!materialized) return 0;
      // Fire read traces
      fireVarTraces(interp, varName, 'read');
      return interp.wrap(materialized);
    },
    feather_host_var_set: (interpId, name, value) => {
      const interp = interpreters.get(interpId);
      const varName = interp.getString(name);
      const frame = interp.currentFrame();
      const entry = frame.vars.get(varName);
      const materialized = interp.materialize(value);
      if (entry?.link) {
        const targetFrame = interp.frames[entry.link.level];
        if (targetFrame) {
          let targetEntry = targetFrame.vars.get(entry.link.name);
          if (!targetEntry) targetEntry = {};
          targetEntry.value = materialized;
          targetFrame.vars.set(entry.link.name, targetEntry);
        }
      } else if (entry?.nsLink) {
        const ns = interp.getNamespace(entry.nsLink.ns);
        if (ns) ns.vars.set(entry.nsLink.name, { value: materialized });
      } else {
        frame.vars.set(varName, { value: materialized });
      }
      // Fire write traces
      fireVarTraces(interp, varName, 'write');
    },
    feather_host_var_unset: (interpId, name) => {
      const interp = interpreters.get(interpId);
      const varName = interp.getString(name);
      interp.currentFrame().vars.delete(varName);
      // Fire unset traces
      fireVarTraces(interp, varName, 'unset');
    },
    feather_host_var_exists: (interpId, name) => {
      const interp = interpreters.get(interpId);
      const varName = interp.getString(name);
      const frame = interp.currentFrame();
      const entry = frame.vars.get(varName);
      if (!entry) return TCL_ERROR;
      if (entry.link) {
        const targetFrame = interp.frames[entry.link.level];
        return targetFrame?.vars.has(entry.link.name) ? TCL_OK : TCL_ERROR;
      }
      if (entry.nsLink) {
        const ns = interp.getNamespace(entry.nsLink.ns);
        return ns?.vars.has(entry.nsLink.name) ? TCL_OK : TCL_ERROR;
      }
      return entry.value !== undefined ? TCL_OK : TCL_ERROR;
    },
    feather_host_var_link: (interpId, local, targetLevel, target) => {
      const interp = interpreters.get(interpId);
      const localName = interp.getString(local);
      const targetName = interp.getString(target);
      interp.currentFrame().vars.set(localName, { link: { level: targetLevel, name: targetName } });
    },
    feather_host_var_link_ns: (interpId, local, ns, name) => {
      const interp = interpreters.get(interpId);
      const localName = interp.getString(local);
      const nsPath = interp.getString(ns);
      const varName = interp.getString(name);
      interp.ensureNamespace(nsPath);
      interp.currentFrame().vars.set(localName, { nsLink: { ns: nsPath, name: varName } });
    },
    feather_host_var_names: (interpId, ns) => {
      const interp = interpreters.get(interpId);
      let names;
      if (ns === 0) {
        // Return local frame variables
        names = [...interp.currentFrame().vars.keys()];
      } else {
        const nsPath = interp.getString(ns);
        const namespace = interp.getNamespace(nsPath);
        names = namespace ? [...namespace.vars.keys()] : [];
      }
      const items = names.map(n => interp.store({ type: 'string', value: n }));
      return interp.store({ type: 'list', items });
    },

    // Proc operations
    feather_host_proc_define: (interpId, name, params, body) => {
      const interp = interpreters.get(interpId);
      const procName = interp.getString(name);
      
      // Materialize for persistent storage
      const materializedParams = interp.materialize(params);
      const materializedBody = interp.materialize(body);
      
      interp.procs.set(procName, { params: materializedParams, body: materializedBody });

      // Also store in namespace commands map for ns_list_commands
      // Extract namespace and simple name from qualified name (e.g., "::foo::bar" -> ns="foo", name="bar")
      let nsPath = '';
      let simpleName = procName;
      if (procName.startsWith('::')) {
        const withoutLeading = procName.slice(2);
        const lastSep = withoutLeading.lastIndexOf('::');
        if (lastSep !== -1) {
          nsPath = withoutLeading.slice(0, lastSep);
          simpleName = withoutLeading.slice(lastSep + 2);
        } else {
          simpleName = withoutLeading;
        }
      }
      const namespace = interp.ensureNamespace('::' + nsPath);
      namespace.commands.set(simpleName, { kind: TCL_CMD_PROC, fn: 0, params: materializedParams, body: materializedBody });
    },
    feather_host_proc_exists: (interpId, name) => {
      const interp = interpreters.get(interpId);
      return interp.procs.has(interp.getString(name)) ? 1 : 0;
    },
    feather_host_proc_params: (interpId, name, resultPtr) => {
      const interp = interpreters.get(interpId);
      const procName = interp.getString(name);
      // Try both qualified and simple names
      const proc = interp.procs.get(procName) || interp.procs.get(`::${procName}`);
      if (proc) {
        writeI32(resultPtr, interp.wrap(proc.params));
        return TCL_OK;
      }
      return TCL_ERROR;
    },
    feather_host_proc_body: (interpId, name, resultPtr) => {
      const interp = interpreters.get(interpId);
      const procName = interp.getString(name);
      // Try both qualified and simple names
      const proc = interp.procs.get(procName) || interp.procs.get(`::${procName}`);
      if (proc) {
        writeI32(resultPtr, interp.wrap(proc.body));
        return TCL_OK;
      }
      return TCL_ERROR;
    },
    feather_host_proc_names: (interpId, namespace) => {
      const interp = interpreters.get(interpId);
      let nsPath = '::';
      if (namespace !== 0) {
        nsPath = interp.getString(namespace);
      }
      const normalized = nsPath.replace(/^::/, '');
      const ns = interp.namespaces.get(normalized);
      const names = [];
      if (ns) {
        for (const [name, entry] of ns.commands) {
          names.push(name);
        }
      }
      // Also include global procs if looking at global namespace
      if (normalized === '') {
        for (const name of interp.procs.keys()) {
          const simpleName = name.replace(/^::/, '');
          if (!simpleName.includes('::') && !names.includes(simpleName)) {
            names.push(simpleName);
          }
        }
        for (const name of interp.builtins.keys()) {
          const simpleName = name.replace(/^::/, '');
          if (!simpleName.includes('::') && !names.includes(simpleName)) {
            names.push(simpleName);
          }
        }
      }
      const items = names.map(n => interp.store({ type: 'string', value: n }));
      return interp.store({ type: 'list', items });
    },
    feather_host_proc_resolve_namespace: (interpId, path, resultPtr) => {
      const interp = interpreters.get(interpId);
      let nsPath = '::';
      if (path !== 0) {
        nsPath = interp.getString(path);
      }
      const normalized = nsPath.replace(/^::/, '');
      if (!interp.namespaces.has(normalized)) return TCL_ERROR;
      writeI32(resultPtr, interp.store({ type: 'string', value: '::' + normalized }));
      return TCL_OK;
    },
    feather_host_proc_register_builtin: (interpId, name, fn) => {
      const interp = interpreters.get(interpId);
      interp.builtins.set(interp.getString(name), fn);
    },
    feather_host_proc_lookup: (interpId, name, fnPtr) => {
      const interp = interpreters.get(interpId);
      const procName = interp.getString(name);
      if (interp.builtins.has(procName)) {
        writeI32(fnPtr, interp.builtins.get(procName));
        return TCL_CMD_BUILTIN;
      }
      if (interp.procs.has(procName)) {
        writeI32(fnPtr, 0);
        return TCL_CMD_PROC;
      }
      writeI32(fnPtr, 0);
      return TCL_CMD_NONE;
    },
    feather_host_proc_rename: (interpId, oldName, newName) => {
      const interp = interpreters.get(interpId);
      const oldN = interp.getString(oldName);
      const newN = interp.getString(newName);

      // Helper to split qualified name into namespace path and simple name
      const splitQualified = (name) => {
        if (name.startsWith('::')) {
          const withoutLeading = name.slice(2);
          const lastSep = withoutLeading.lastIndexOf('::');
          if (lastSep !== -1) {
            return [withoutLeading.slice(0, lastSep), withoutLeading.slice(lastSep + 2)];
          }
          return ['', withoutLeading]; // global namespace
        }
        return ['', name]; // unqualified = global
      };

      const [oldNsPath, oldSimple] = splitQualified(oldN);
      const oldNs = interp.namespaces.get(oldNsPath);

      // Check if command exists in namespace
      if (!oldNs?.commands.has(oldSimple)) {
        // Also check legacy procs/builtins maps
        if (!interp.procs.has(oldN) && !interp.builtins.has(oldN)) {
          interp.result = interp.store({ type: 'string', value: `can't rename "${displayName(oldN)}": command doesn't exist` });
          return TCL_ERROR;
        }
      }

      // Check if target already exists (before we do anything)
      if (newN) {
        const [newNsPath, newSimple] = splitQualified(newN);
        const newNs = interp.namespaces.get(newNsPath);
        if (newNs?.commands.has(newSimple)) {
          interp.result = interp.store({ type: 'string', value: `can't rename to "${displayName(newN)}": command already exists` });
          return TCL_ERROR;
        }
      }

      // Determine the operation for trace firing
      const op = newN ? 'rename' : 'delete';

      // Get the command from namespace or legacy maps
      let cmd = oldNs?.commands.get(oldSimple);
      if (!cmd && interp.procs.has(oldN)) {
        const proc = interp.procs.get(oldN);
        cmd = { kind: TCL_CMD_PROC, fn: 0, params: proc.params, body: proc.body };
      } else if (!cmd && interp.builtins.has(oldN)) {
        cmd = { kind: TCL_CMD_BUILTIN, fn: interp.builtins.get(oldN) };
      }

      if (!cmd) return TCL_ERROR;

      // Fire command traces before the rename/delete
      fireCmdTraces(interp, oldN, newN, op);

      // Delete from old location
      if (oldNs) oldNs.commands.delete(oldSimple);
      interp.procs.delete(oldN);
      interp.builtins.delete(oldSimple);

      // Add to new location if not deleting
      if (newN) {
        const [newNsPath, newSimple] = splitQualified(newN);
        const newNs = interp.ensureNamespace('::' + newNsPath);
        newNs.commands.set(newSimple, cmd);

        // Also update legacy maps
        if (cmd.kind === TCL_CMD_PROC) {
          interp.procs.set(newN, { params: cmd.params, body: cmd.body });
        } else if (cmd.kind === TCL_CMD_BUILTIN) {
          interp.builtins.set(newSimple, cmd.fn);
        }
      }

      return TCL_OK;
    },

    // Namespace operations
    feather_host_ns_create: (interpId, path) => {
      const interp = interpreters.get(interpId);
      interp.ensureNamespace(interp.getString(path));
      return TCL_OK;
    },
    feather_host_ns_delete: (interpId, path) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(path);
      if (nsPath === '::' || nsPath === '') return TCL_ERROR;
      const normalized = nsPath.replace(/^::/, '');
      if (!interp.namespaces.has(normalized)) return TCL_ERROR;
      // Delete all children recursively
      const toDelete = [normalized];
      for (const [key] of interp.namespaces) {
        if (key.startsWith(normalized + '::')) {
          toDelete.push(key);
        }
      }
      for (const key of toDelete) {
        interp.namespaces.delete(key);
      }
      return TCL_OK;
    },
    feather_host_ns_exists: (interpId, path) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(path);
      const normalized = nsPath.replace(/^::/, '');
      return interp.namespaces.has(normalized) ? 1 : 0;
    },
    feather_host_ns_current: (interpId) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'string', value: interp.currentFrame().ns });
    },
    feather_host_ns_parent: (interpId, ns, resultPtr) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns);
      if (nsPath === '::' || nsPath === '') {
        writeI32(resultPtr, interp.store({ type: 'string', value: '' }));
        return TCL_OK;
      }
      const normalized = nsPath.replace(/^::/, '');
      const lastSep = normalized.lastIndexOf('::');
      const parent = lastSep >= 0 ? '::' + normalized.slice(0, lastSep) : '::';
      writeI32(resultPtr, interp.store({ type: 'string', value: parent }));
      return TCL_OK;
    },
    feather_host_ns_children: (interpId, ns) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns);
      const normalized = nsPath.replace(/^::/, '');
      const namespace = interp.namespaces.get(normalized);
      if (!namespace) return interp.store({ type: 'list', items: [] });
      const children = [...namespace.children.values()].map(c => '::' + c).sort();
      const items = children.map(c => interp.store({ type: 'string', value: c }));
      return interp.store({ type: 'list', items });
    },
    feather_host_ns_get_var: (interpId, ns, name) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns);
      const varName = interp.getString(name);
      const namespace = interp.getNamespace(nsPath);
      if (!namespace) return 0;
      const entry = namespace.vars.get(varName);
      if (!entry) return 0;
      const materialized = typeof entry === 'object' && 'value' in entry ? entry.value : entry;
      if (!materialized) return 0;
      return interp.wrap(materialized);
    },
    feather_host_ns_set_var: (interpId, ns, name, value) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns);
      const varName = interp.getString(name);
      const namespace = interp.ensureNamespace(nsPath);
      const materialized = interp.materialize(value);
      namespace.vars.set(varName, { value: materialized });
    },
    feather_host_ns_var_exists: (interpId, ns, name) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns);
      const varName = interp.getString(name);
      const namespace = interp.getNamespace(nsPath);
      return namespace?.vars.has(varName) ? 1 : 0;
    },
    feather_host_ns_unset_var: (interpId, ns, name) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns);
      const varName = interp.getString(name);
      const namespace = interp.getNamespace(nsPath);
      if (namespace) namespace.vars.delete(varName);
    },
    feather_host_ns_get_command: (interpId, ns, name, fnPtr) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns);
      const cmdName = interp.getString(name);
      const normalized = nsPath.replace(/^::/, '');
      const namespace = interp.namespaces.get(normalized);
      if (!namespace) {
        writeI32(fnPtr, 0);
        return TCL_CMD_NONE;
      }
      const cmd = namespace.commands.get(cmdName);
      if (!cmd) {
        writeI32(fnPtr, 0);
        return TCL_CMD_NONE;
      }
      writeI32(fnPtr, cmd.fn || 0);
      return cmd.kind;
    },
    feather_host_ns_set_command: (interpId, ns, name, kind, fn, params, body) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns);
      const cmdName = interp.getString(name);
      const namespace = interp.ensureNamespace(nsPath);
      namespace.commands.set(cmdName, { 
        kind, 
        fn, 
        params: interp.materialize(params),
        body: interp.materialize(body)
      });
    },
    feather_host_ns_delete_command: (interpId, ns, name) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns);
      const cmdName = interp.getString(name);
      const normalized = nsPath.replace(/^::/, '');
      const namespace = interp.namespaces.get(normalized);
      if (!namespace || !namespace.commands.has(cmdName)) return TCL_ERROR;
      namespace.commands.delete(cmdName);
      return TCL_OK;
    },
    feather_host_ns_list_commands: (interpId, ns) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns);
      const normalized = nsPath.replace(/^::/, '');
      const namespace = interp.namespaces.get(normalized);
      if (!namespace) return interp.store({ type: 'list', items: [] });
      const names = [...namespace.commands.keys()];
      const items = names.map(n => interp.store({ type: 'string', value: n }));
      return interp.store({ type: 'list', items });
    },
    feather_host_ns_get_exports: (interpId, ns) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns);
      const normalized = nsPath.replace(/^::/, '');
      const namespace = interp.namespaces.get(normalized);
      if (!namespace) return interp.store({ type: 'list', items: [] });
      const items = namespace.exports.map(p => interp.store({ type: 'string', value: p }));
      return interp.store({ type: 'list', items });
    },
    feather_host_ns_set_exports: (interpId, ns, patterns, clear) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns);
      const normalized = nsPath.replace(/^::/, '');
      const namespace = interp.ensureNamespace(nsPath);
      const patList = interp.getList(patterns).items.map(h => interp.getString(h));
      if (clear) {
        namespace.exports = patList;
      } else {
        namespace.exports.push(...patList);
      }
    },
    feather_host_ns_is_exported: (interpId, ns, name) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns);
      const cmdName = interp.getString(name);
      const normalized = nsPath.replace(/^::/, '');
      const namespace = interp.namespaces.get(normalized);
      if (!namespace) return 0;
      return namespace.exports.some(p => globMatch(p, cmdName)) ? 1 : 0;
    },
    feather_host_ns_copy_command: (interpId, srcNs, srcName, dstNs, dstName) => {
      const interp = interpreters.get(interpId);
      const srcNsPath = interp.getString(srcNs).replace(/^::/, '');
      const dstNsPath = interp.getString(dstNs).replace(/^::/, '');
      const src = interp.getString(srcName);
      const dst = interp.getString(dstName);

      const srcNamespace = interp.namespaces.get(srcNsPath);
      const dstNamespace = interp.ensureNamespace('::' + dstNsPath);

      // Copy from namespace commands
      if (srcNamespace?.commands.has(src)) {
        const cmd = srcNamespace.commands.get(src);
        dstNamespace.commands.set(dst, { ...cmd });
        // Also update legacy maps
        if (cmd.kind === TCL_CMD_BUILTIN) {
          interp.builtins.set(dst, cmd.fn);
        } else if (cmd.kind === TCL_CMD_PROC) {
          const fullName = dstNsPath === '' ? `::${dst}` : `::${dstNsPath}::${dst}`;
          interp.procs.set(fullName, { params: cmd.params, body: cmd.body });
        }
        return TCL_OK;
      }
      return TCL_ERROR;
    },

    // String operations
    feather_host_string_intern: (interpId, ptr, len) => {
      const interp = interpreters.get(interpId);
      const str = readString(ptr, len);
      return interp.store({ type: 'string', value: str });
    },
    feather_host_string_get: (interpId, obj, lenPtr) => {
      const interp = interpreters.get(interpId);
      const str = interp.getString(obj);
      writeI32(lenPtr, str.length);
      const [ptr] = writeString(str);
      return ptr;
    },
    feather_host_string_concat: (interpId, a, b) => {
      const interp = interpreters.get(interpId);
      const str = interp.getString(a) + interp.getString(b);
      return interp.store({ type: 'string', value: str });
    },
    feather_host_string_compare: (interpId, a, b) => {
      const interp = interpreters.get(interpId);
      const strA = interp.getString(a);
      const strB = interp.getString(b);
      return strA < strB ? -1 : strA > strB ? 1 : 0;
    },
    feather_host_string_regex_match: (interpId, pattern, string, resultPtr) => {
      const interp = interpreters.get(interpId);
      const patStr = interp.getString(pattern);
      const strVal = interp.getString(string);
      try {
        const re = new RegExp(patStr);
        writeI32(resultPtr, re.test(strVal) ? 1 : 0);
        return TCL_OK;
      } catch (e) {
        interp.result = interp.store({ type: 'string', value: `invalid regex: ${e.message}` });
        return TCL_ERROR;
      }
    },

    // Rune operations
    feather_host_rune_length: (interpId, str) => {
      const interp = interpreters.get(interpId);
      return [...interp.getString(str)].length;
    },
    feather_host_rune_at: (interpId, str, index) => {
      const interp = interpreters.get(interpId);
      const chars = [...interp.getString(str)];
      if (index >= chars.length) return interp.store({ type: 'string', value: '' });
      return interp.store({ type: 'string', value: chars[index] });
    },
    feather_host_rune_range: (interpId, str, first, last) => {
      const interp = interpreters.get(interpId);
      const chars = [...interp.getString(str)];
      const f = Math.max(0, Number(first));
      const l = Math.min(chars.length - 1, Number(last));
      if (f > l) return interp.store({ type: 'string', value: '' });
      return interp.store({ type: 'string', value: chars.slice(f, l + 1).join('') });
    },
    feather_host_rune_to_upper: (interpId, str) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'string', value: interp.getString(str).toUpperCase() });
    },
    feather_host_rune_to_lower: (interpId, str) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'string', value: interp.getString(str).toLowerCase() });
    },
    feather_host_rune_fold: (interpId, str) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'string', value: interp.getString(str).toLowerCase() });
    },

    // List operations
    feather_host_list_is_nil: (interpId, obj) => {
      return obj === 0 ? 1 : 0;
    },
    feather_host_list_create: (interpId) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'list', items: [] });
    },
    feather_host_list_from: (interpId, obj) => {
      const interp = interpreters.get(interpId);
      const existingObj = interp.get(obj);
      if (existingObj?.type === 'list') {
        // Already a list - just copy
        return interp.store({ type: 'list', items: [...existingObj.items] });
      }
      // Need to parse string as list - use C's feather_list_parse
      const str = interp.getString(obj);
      const [ptr, len] = writeString(str);
      const listHandle = wasmInstance.exports.feather_list_parse(0, interpId, ptr, len);
      wasmInstance.exports.free(ptr);

      if (listHandle === 0) {
        // Parse error - error message already set in interp.result by C code
        return 0;
      }

      // The C function returned a list handle - it's already registered in interp
      // But we need to copy it to avoid mutation issues
      const parsedObj = interp.get(listHandle);
      if (parsedObj?.type === 'list') {
        return interp.store({ type: 'list', items: [...parsedObj.items] });
      }
      return listHandle;
    },
    feather_host_list_push: (interpId, list, item) => {
      const interp = interpreters.get(interpId);
      const obj = interp.get(list);
      if (obj?.type === 'list') {
        obj.items.push(item);
        interp.invalidateStringCache(obj);
        return list;
      }
      const result = interp.getList(list);
      const items = result.items;
      items.push(item);
      return interp.store({ type: 'list', items });
    },
    feather_host_list_pop: (interpId, list) => {
      const interp = interpreters.get(interpId);
      const obj = interp.get(list);
      if (obj?.type === 'list' && obj.items.length > 0) {
        interp.invalidateStringCache(obj);
        return obj.items.pop();
      }
      return 0;
    },
    feather_host_list_unshift: (interpId, list, item) => {
      const interp = interpreters.get(interpId);
      const obj = interp.get(list);
      if (obj?.type === 'list') {
        obj.items.unshift(item);
        interp.invalidateStringCache(obj);
        return list;
      }
      const result = interp.getList(list);
      const items = result.items;
      items.unshift(item);
      return interp.store({ type: 'list', items });
    },
    feather_host_list_shift: (interpId, list) => {
      const interp = interpreters.get(interpId);
      const obj = interp.get(list);
      if (obj?.type === 'list' && obj.items.length > 0) {
        interp.invalidateStringCache(obj);
        return obj.items.shift();
      }
      return 0;
    },
    feather_host_list_length: (interpId, list) => {
      const interp = interpreters.get(interpId);
      return interp.getList(list).items.length;
    },
    feather_host_list_at: (interpId, list, index) => {
      const interp = interpreters.get(interpId);
      const items = interp.getList(list).items;
      return items[index] || 0;
    },
    feather_host_list_slice: (interpId, list, first, last) => {
      const interp = interpreters.get(interpId);
      const items = interp.getList(list).items;
      return interp.store({ type: 'list', items: items.slice(first, last + 1) });
    },
    feather_host_list_set_at: (interpId, list, index, value) => {
      const interp = interpreters.get(interpId);
      const obj = interp.get(list);
      if (obj?.type === 'list') {
        if (index >= obj.items.length) return TCL_ERROR;
        obj.items[index] = value;
        interp.invalidateStringCache(obj);
        return TCL_OK;
      }
      return TCL_ERROR;
    },
    feather_host_list_splice: (interpId, list, first, deleteCount, insertions) => {
      const interp = interpreters.get(interpId);
      const obj = interp.get(list);
      if (obj?.type === 'list') {
        const insItems = interp.getList(insertions).items;
        obj.items.splice(first, deleteCount, ...insItems);
        interp.invalidateStringCache(obj);
        return list;
      }
      const items = interp.getList(list).items;
      const insItems = interp.getList(insertions).items;
      items.splice(first, deleteCount, ...insItems);
      return interp.store({ type: 'list', items });
    },
    feather_host_list_sort: (interpId, list, cmpFn, ctx) => {
      const interp = interpreters.get(interpId);
      const obj = interp.get(list);
      if (obj?.type !== 'list') return TCL_ERROR;
      if (obj.items.length <= 1) return TCL_OK;
      // Sort using the C comparison function via exported helper
      obj.items.sort((a, b) => {
        return wasmInstance.exports.wasm_call_compare(interpId, a, b, cmpFn, ctx);
      });
      interp.invalidateStringCache(obj);
      return TCL_OK;
    },

    // Dict operations
    feather_host_dict_create: (interpId) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'dict', entries: [] });
    },
    feather_host_dict_is_dict: (interpId, obj) => {
      const interp = interpreters.get(interpId);
      const o = interp.get(obj);
      return o?.type === 'dict' ? 1 : 0;
    },
    feather_host_dict_from: (interpId, obj) => {
      const interp = interpreters.get(interpId);
      const dict = interp.getDict(obj);
      if (!dict) return 0;
      return interp.store({ type: 'dict', entries: [...dict.entries] });
    },
    feather_host_dict_get: (interpId, dict, key) => {
      const interp = interpreters.get(interpId);
      const d = interp.getDict(dict);
      if (!d) return 0;
      const keyStr = interp.getString(key);
      for (const [k, v] of d.entries) {
        if (interp.getString(k) === keyStr) return v;
      }
      return 0;
    },
    feather_host_dict_set: (interpId, dict, key, value) => {
      const interp = interpreters.get(interpId);
      const obj = interp.get(dict);
      if (obj?.type !== 'dict') {
        const d = interp.getDict(dict);
        if (!d) return interp.store({ type: 'dict', entries: [[key, value]] });
        const entries = [...d.entries];
        const keyStr = interp.getString(key);
        let found = false;
        for (let i = 0; i < entries.length; i++) {
          if (interp.getString(entries[i][0]) === keyStr) {
            entries[i][1] = value;
            found = true;
            break;
          }
        }
        if (!found) entries.push([key, value]);
        return interp.store({ type: 'dict', entries });
      }
      const keyStr = interp.getString(key);
      let found = false;
      for (let i = 0; i < obj.entries.length; i++) {
        if (interp.getString(obj.entries[i][0]) === keyStr) {
          obj.entries[i][1] = value;
          found = true;
          break;
        }
      }
      if (!found) obj.entries.push([key, value]);
      return dict;
    },
    feather_host_dict_exists: (interpId, dict, key) => {
      const interp = interpreters.get(interpId);
      const d = interp.getDict(dict);
      if (!d) return 0;
      const keyStr = interp.getString(key);
      for (const [k] of d.entries) {
        if (interp.getString(k) === keyStr) return 1;
      }
      return 0;
    },
    feather_host_dict_remove: (interpId, dict, key) => {
      const interp = interpreters.get(interpId);
      const dictData = interp.getDict(dict);
      if (!dictData) return dict;
      const dictObj = interp.get(dict);
      const keyStr = interp.getString(key);
      dictObj.entries = dictObj.entries.filter(([k]) => interp.getString(k) !== keyStr);
      return dict;
    },
    feather_host_dict_size: (interpId, dict) => {
      const interp = interpreters.get(interpId);
      const d = interp.getDict(dict);
      return d ? d.entries.length : 0;
    },
    feather_host_dict_keys: (interpId, dict) => {
      const interp = interpreters.get(interpId);
      const d = interp.getDict(dict);
      if (!d) return interp.store({ type: 'list', items: [] });
      return interp.store({ type: 'list', items: d.entries.map(([k]) => k) });
    },
    feather_host_dict_values: (interpId, dict) => {
      const interp = interpreters.get(interpId);
      const d = interp.getDict(dict);
      if (!d) return interp.store({ type: 'list', items: [] });
      return interp.store({ type: 'list', items: d.entries.map(([, v]) => v) });
    },

    // Integer operations
    feather_host_integer_create: (interpId, val) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'int', value: Number(val) });
    },
    feather_host_integer_get: (interpId, obj, outPtr) => {
      const interp = interpreters.get(interpId);
      const o = interp.get(obj);
      if (o?.type === 'int') {
        writeI64(outPtr, o.value);
        return TCL_OK;
      }
      const str = interp.getString(obj).trim();
      // Must be a valid integer string (not a float that parseInt would truncate)
      if (!/^-?(?:0x[0-9a-fA-F]+|0o[0-7]+|0b[01]+|[0-9]+)$/.test(str)) {
        interp.result = interp.store({ type: 'string', value: `expected integer but got "${str}"` });
        return TCL_ERROR;
      }
      const val = parseInt(str, str.startsWith('0x') || str.startsWith('-0x') ? 16 :
                           str.startsWith('0o') || str.startsWith('-0o') ? 8 :
                           str.startsWith('0b') || str.startsWith('-0b') ? 2 : 10);
      if (isNaN(val)) {
        interp.result = interp.store({ type: 'string', value: `expected integer but got "${str}"` });
        return TCL_ERROR;
      }
      writeI64(outPtr, val);
      return TCL_OK;
    },

    // Double operations
    feather_host_dbl_create: (interpId, val) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'double', value: val });
    },
    feather_host_dbl_get: (interpId, obj, outPtr) => {
      const interp = interpreters.get(interpId);
      const o = interp.get(obj);
      if (o?.type === 'double') {
        writeF64(outPtr, o.value);
        return TCL_OK;
      }
      if (o?.type === 'int') {
        writeF64(outPtr, o.value);
        return TCL_OK;
      }
      const str = interp.getString(obj).trim();
      // Handle special values first
      if (str === 'NaN') {
        writeF64(outPtr, NaN);
        return TCL_OK;
      }
      if (str === '+Inf' || str === 'Inf') {
        writeF64(outPtr, Infinity);
        return TCL_OK;
      }
      if (str === '-Inf') {
        writeF64(outPtr, -Infinity);
        return TCL_OK;
      }
      // Must be a valid numeric string (parseFloat is too lenient - "0y" parses as 0)
      if (!/^-?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?$/.test(str)) {
        interp.result = interp.store({ type: 'string', value: `expected floating-point number but got "${str}"` });
        return TCL_ERROR;
      }
      const val = parseFloat(str);
      writeF64(outPtr, val);
      return TCL_OK;
    },
    feather_host_dbl_classify: (val) => {
      if (Number.isNaN(val)) return 4;  // FEATHER_DBL_NAN
      if (val === Infinity) return 2;   // FEATHER_DBL_INF
      if (val === -Infinity) return 3;  // FEATHER_DBL_NEG_INF
      if (val === 0) return 1;          // FEATHER_DBL_ZERO
      return 0;                         // FEATHER_DBL_NORMAL
    },
    feather_host_dbl_format: (interpId, val, specifier, precision) => {
      const interp = interpreters.get(interpId);
      const spec = String.fromCharCode(specifier);
      const prec = precision < 0 ? 6 : precision;

      // Handle special values
      if (Number.isNaN(val)) {
        return interp.store({ type: 'string', value: 'NaN' });
      }
      if (val === Infinity) {
        return interp.store({ type: 'string', value: 'Inf' });
      }
      if (val === -Infinity) {
        return interp.store({ type: 'string', value: '-Inf' });
      }

      // Helper to pad exponent to at least 2 digits (TCL format)
      const padExponent = (s) => {
        return s.replace(/e([+-])(\d)$/, 'e$10$2');
      };

      // Format based on specifier
      let result;
      switch (spec) {
        case 'e':
        case 'E':
          result = padExponent(val.toExponential(prec));
          if (spec === 'E') result = result.toUpperCase();
          break;
        case 'f':
        case 'F':
          result = val.toFixed(prec);
          break;
        case 'g':
        case 'G':
        default: {
          // TCL %g: use exponential if exponent < -4 or >= precision
          const absVal = Math.abs(val);
          let exponent = 0;
          if (absVal !== 0) {
            exponent = Math.floor(Math.log10(absVal));
          }
          const sigfigs = prec > 0 ? prec : 6;
          
          if (exponent < -4 || exponent >= sigfigs) {
            // Use exponential notation
            result = val.toExponential(sigfigs - 1);
            // Trim trailing zeros before 'e' (TCL %g behavior)
            result = result.replace(/(\.\d*?)0+(e)/, '$1$2').replace(/\.(e)/, '$1');
            result = padExponent(result);
          } else {
            // Use fixed notation with trailing zeros trimmed
            const decimalPlaces = sigfigs - exponent - 1;
            result = val.toFixed(Math.max(0, decimalPlaces));
            // Remove trailing zeros after decimal point
            if (result.includes('.')) {
              result = result.replace(/\.?0+$/, '');
            }
          }
          if (spec === 'G') result = result.toUpperCase();
          break;
        }
      }
      return interp.store({ type: 'string', value: result });
    },
    feather_host_dbl_math: (interpId, op, a, b, outPtr) => {
      const interp = interpreters.get(interpId);
      let result;
      switch (op) {
        case 0:  result = Math.sqrt(a); break;    // FEATHER_MATH_SQRT
        case 1:  result = Math.exp(a); break;     // FEATHER_MATH_EXP
        case 2:  result = Math.log(a); break;     // FEATHER_MATH_LOG
        case 3:  result = Math.log10(a); break;   // FEATHER_MATH_LOG10
        case 4:  result = Math.sin(a); break;     // FEATHER_MATH_SIN
        case 5:  result = Math.cos(a); break;     // FEATHER_MATH_COS
        case 6:  result = Math.tan(a); break;     // FEATHER_MATH_TAN
        case 7:  result = Math.asin(a); break;    // FEATHER_MATH_ASIN
        case 8:  result = Math.acos(a); break;    // FEATHER_MATH_ACOS
        case 9:  result = Math.atan(a); break;    // FEATHER_MATH_ATAN
        case 10: result = Math.sinh(a); break;    // FEATHER_MATH_SINH
        case 11: result = Math.cosh(a); break;    // FEATHER_MATH_COSH
        case 12: result = Math.tanh(a); break;    // FEATHER_MATH_TANH
        case 13: result = Math.floor(a); break;   // FEATHER_MATH_FLOOR
        case 14: result = Math.ceil(a); break;    // FEATHER_MATH_CEIL
        case 15: result = a < 0 ? -Math.round(-a) : Math.round(a); break;   // FEATHER_MATH_ROUND (away from zero)
        case 16: result = Math.abs(a); break;     // FEATHER_MATH_ABS
        case 17: result = Math.pow(a, b); break;  // FEATHER_MATH_POW
        case 18: result = Math.atan2(a, b); break;// FEATHER_MATH_ATAN2
        case 19: result = a % b; break;           // FEATHER_MATH_FMOD
        case 20: result = Math.hypot(a, b); break;// FEATHER_MATH_HYPOT
        default:
          interp.result = interp.store({ type: 'string', value: 'unknown math operation' });
          return TCL_ERROR;
      }
      writeF64(outPtr, result);
      return TCL_OK;
    },

    // Interp operations
    feather_host_interp_set_result: (interpId, result) => {
      const interp = interpreters.get(interpId);
      interp.result = result;
      return TCL_OK;
    },
    feather_host_interp_get_result: (interpId) => {
      return interpreters.get(interpId).result;
    },
    feather_host_interp_reset_result: (interpId, result) => {
      const interp = interpreters.get(interpId);
      interp.result = result;
      interp.returnOptions.clear();
      return TCL_OK;
    },
    feather_host_interp_set_return_options: (interpId, options) => {
      const interp = interpreters.get(interpId);
      interp.returnOptions.set('current', options);
      // Parse options to extract -code
      const items = interp.getList(options).items;
      let code = TCL_OK;
      let level = 1;
      for (let i = 0; i + 1 < items.length; i += 2) {
        const key = interp.getString(items[i]);
        const val = interp.getString(items[i + 1]);
        if (key === '-code') {
          if (val === 'ok') code = TCL_OK;
          else if (val === 'error') code = TCL_ERROR;
          else if (val === 'return') code = TCL_RETURN;
          else if (val === 'break') code = TCL_BREAK;
          else if (val === 'continue') code = TCL_CONTINUE;
          else code = parseInt(val, 10) || TCL_OK;
        }
        if (key === '-level') {
          level = parseInt(val, 10) || 1;
        }
      }
      // Return TCL_RETURN if level > 0, else the final code
      if (level > 0) {
        return TCL_RETURN;
      }
      return code;
    },
    feather_host_interp_get_return_options: (interpId, code) => {
      const interp = interpreters.get(interpId);
      const current = interp.returnOptions.get('current');
      if (current) return current;
      // Build default options based on code
      const items = [
        interp.store({ type: 'string', value: '-code' }),
        interp.store({ type: 'string', value: String(code) }),
        interp.store({ type: 'string', value: '-level' }),
        interp.store({ type: 'string', value: '0' }),
      ];
      return interp.store({ type: 'list', items });
    },
    feather_host_interp_get_script: (interpId) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'string', value: interp.scriptPath });
    },
    feather_host_interp_set_script: (interpId, path) => {
      const interp = interpreters.get(interpId);
      interp.scriptPath = interp.getString(path);
    },

    // Bind operations
    feather_host_bind_unknown: (interpId, cmd, args, valuePtr) => {
      const interp = interpreters.get(interpId);
      const cmdName = interp.getString(cmd);
      const hostFn = interp.hostCommands.get(cmdName);
      if (!hostFn) {
        interp.result = interp.store({ type: 'string', value: `invalid command name "${cmdName}"` });
        return TCL_ERROR;
      }

      const argList = interp.getList(args).items.map(h => interp.getString(h));
      try {
        const result = hostFn(argList);
        const handle = interp.store({ type: 'string', value: String(result ?? '') });
        writeI32(valuePtr, handle);
        return TCL_OK;
      } catch (e) {
        interp.result = interp.store({ type: 'string', value: e.message });
        return TCL_ERROR;
      }
    },

    // Trace operations
    feather_host_trace_add: (interpId, kind, name, ops, script) => {
      const interp = interpreters.get(interpId);
      const kindStr = interp.getString(kind);
      const nameStr = interp.getString(name);
      const opsStr = interp.getString(ops);
      // Materialize script as string for persistent storage
      const scriptStr = interp.getString(script);
      const traces = interp.traces[kindStr];
      if (!traces) return TCL_ERROR;
      if (!traces.has(nameStr)) traces.set(nameStr, []);
      traces.get(nameStr).push({ ops: opsStr, script: scriptStr });
      return TCL_OK;
    },
    feather_host_trace_remove: (interpId, kind, name, ops, script) => {
      const interp = interpreters.get(interpId);
      const kindStr = interp.getString(kind);
      const nameStr = interp.getString(name);
      const opsStr = interp.getString(ops);
      const traces = interp.traces[kindStr]?.get(nameStr);
      if (!traces) return TCL_ERROR;
      const scriptStr = interp.getString(script);
      // trace.script is now stored as string, compare directly
      const idx = traces.findIndex(t => t.ops === opsStr && t.script === scriptStr);
      if (idx >= 0) {
        traces.splice(idx, 1);
        return TCL_OK;
      }
      return TCL_ERROR;
    },
    feather_host_trace_info: (interpId, kind, name) => {
      const interp = interpreters.get(interpId);
      const kindStr = interp.getString(kind);
      const nameStr = interp.getString(name);
      const traces = interp.traces[kindStr]?.get(nameStr) || [];
      const items = traces.map(t => {
        // Split ops into individual elements, then append script
        const ops = t.ops.split(/\s+/).filter(o => o);
        const subItems = ops.map(op => interp.store({ type: 'string', value: op }));
        // t.script is now stored as string, wrap it as a handle
        subItems.push(interp.store({ type: 'string', value: t.script }));
        return interp.store({ type: 'list', items: subItems });
      });
      return interp.store({ type: 'list', items });
    },

    // Foreign object operations
    feather_host_foreign_is_foreign: (interpId, obj) => {
      const interp = interpreters.get(interpId);
      return interp.isForeign(obj) ? 1 : 0;
    },
    feather_host_foreign_type_name: (interpId, obj) => {
      const interp = interpreters.get(interpId);
      const typeName = interp.getForeignTypeName(obj);
      if (!typeName) return 0;
      return interp.store({ type: 'string', value: typeName });
    },
    feather_host_foreign_string_rep: (interpId, obj) => {
      const interp = interpreters.get(interpId);
      const instance = interp.getForeignInstance(obj);
      if (!instance) return 0;
      return interp.store({ type: 'string', value: instance.handleName || `<${instance.typeName}:${obj}>` });
    },
    feather_host_foreign_methods: (interpId, obj) => {
      const interp = interpreters.get(interpId);
      const typeName = interp.getForeignTypeName(obj);
      if (!typeName) return interp.store({ type: 'list', items: [] });
      const typeDef = interp.foreignTypes.get(typeName);
      if (!typeDef) return interp.store({ type: 'list', items: [] });
      const methods = Object.keys(typeDef.methods || {}).concat(['destroy']);
      return interp.store({ type: 'list', items: methods.map(m => interp.store({ type: 'string', value: m })) });
    },
    feather_host_foreign_invoke: (interpId, obj, method, args) => {
      const interp = interpreters.get(interpId);
      const o = interp.get(obj);
      if (o?.type !== 'foreign') return TCL_ERROR;
      const typeDef = interp.foreignTypes.get(o.typeName);
      const methodName = interp.getString(method);
      const fn = typeDef?.methods?.[methodName];
      if (!fn) {
        interp.result = interp.store({ type: 'string', value: `unknown method "${methodName}"` });
        return TCL_ERROR;
      }
      try {
        const argList = interp.getList(args).items.map(h => interp.getString(h));
        const result = fn(o.value, ...argList);
        interp.result = interp.store({ type: 'string', value: String(result ?? '') });
        return TCL_OK;
      } catch (e) {
        interp.result = interp.store({ type: 'string', value: e.message });
        return TCL_ERROR;
      }
    },
    feather_host_foreign_destroy: (interpId, obj) => {
      const interp = interpreters.get(interpId);
      const o = interp.get(obj);
      if (o?.type !== 'foreign') return;
      const typeDef = interp.foreignTypes.get(o.typeName);
      typeDef?.destroy?.(o.value);
    },
  };

  let wasmBytes;
  if (typeof wasmSource === 'string') {
    const isNode = typeof process !== 'undefined' && process.versions?.node;
    const isAbsolutePath = wasmSource.startsWith('/') || /^[A-Za-z]:/.test(wasmSource);
    const isRelativePath = wasmSource.startsWith('./') || wasmSource.startsWith('../');

    if (isNode && (isAbsolutePath || isRelativePath)) {
      const { readFileSync } = await import('fs');
      wasmBytes = readFileSync(wasmSource);
    } else {
      const response = await fetch(wasmSource);
      wasmBytes = await response.arrayBuffer();
    }
  } else if (wasmSource instanceof ArrayBuffer || ArrayBuffer.isView(wasmSource)) {
    wasmBytes = wasmSource;
  } else if (wasmSource instanceof Response) {
    wasmBytes = await wasmSource.arrayBuffer();
  } else {
    throw new Error('wasmSource must be a path, URL, ArrayBuffer, or Response');
  }

  const wasmModule = await WebAssembly.compile(wasmBytes);
  wasmMemory = new WebAssembly.Memory({ initial: 32 });

  wasmInstance = await WebAssembly.instantiate(wasmModule, {
    env: {
      memory: wasmMemory,
      ...hostImports,
    }
  });

  wasmMemory = wasmInstance.exports.memory || wasmMemory;

  return {
    create() {
      const id = nextInterpId++;
      const interp = new FeatherInterp(id);
      interpreters.set(id, interp);

      // Inject the C list parser function
      interp._parseListFromC = (str) => {
        const [ptr, len] = writeString(str);
        const result = wasmInstance.exports.feather_list_parse(0, id, ptr, len);
        wasmInstance.exports.free(ptr);
        return result;
      };

      wasmInstance.exports.feather_interp_init(0, id);
      return id;
    },

    register(interpId, name, fn) {
      interpreters.get(interpId).hostCommands.set(name, fn);
    },

    registerType(interpId, typeName, typeDef) {
      interpreters.get(interpId).foreignTypes.set(typeName, typeDef);
    },

    createForeign(interpId, typeName, value, handleName) {
      const interp = interpreters.get(interpId);
      const objHandle = interp.store({ type: 'foreign', typeName, value, stringRep: handleName });
      if (handleName) {
        interp.foreignInstances.set(handleName, { typeName, value, objHandle, handleName });
        // Register a command for this instance that dispatches to methods
        const typeDef = interp.foreignTypes.get(typeName);
        interp.hostCommands.set(handleName, (args) => {
          const method = args[0];
          if (!method) throw new Error(`wrong # args: should be "${handleName} method ?arg ...?"`);
          if (method === 'destroy') {
            interp.foreignInstances.delete(handleName);
            interp.hostCommands.delete(handleName);
            return '';
          }
          const methodFn = typeDef?.methods?.[method];
          if (!methodFn) {
            const available = Object.keys(typeDef?.methods || {}).concat('destroy').join(', ');
            throw new Error(`unknown method "${method}": must be ${available}`);
          }
          return methodFn(value, ...args.slice(1));
        });
      }
      return objHandle;
    },

    destroyForeign(interpId, handleName) {
      const interp = interpreters.get(interpId);
      interp.foreignInstances.delete(handleName);
    },

    parse(interpId, script) {
      const interp = interpreters.get(interpId);
      const [ptr, len] = writeString(script);

      // Allocate FeatherParseContext (3 x 4 bytes for wasm32)
      const ctxPtr = wasmInstance.exports.alloc(12);
      wasmInstance.exports.feather_parse_init(ctxPtr, ptr, len);

      const status = wasmInstance.exports.feather_parse_command(0, interpId, ctxPtr);

      // Note: don't free ctxPtr/ptr - they're in arena, will be reset

      // Convert TCL_PARSE_DONE to TCL_PARSE_OK (empty script is OK)
      if (status === TCL_PARSE_DONE) {
        interp.result = interp.store({ type: 'list', items: [] });
        // Reset arenas (parse is always top-level)
        interp.resetScratch();
        wasmInstance.exports.feather_arena_reset();
        return { status: TCL_PARSE_OK, result: '' };
      }

      // Get result string (for INCOMPLETE/ERROR: "{TYPE pos len}" or "{ERROR pos len {msg}}")
      const resultStr = interp.getString(interp.result);

      // For errors, extract the error message (4th element)
      let errorMessage = '';
      if (status === TCL_PARSE_ERROR) {
        const resultObj = interp.get(interp.result);
        if (resultObj?.type === 'list' && resultObj.items.length >= 4) {
          errorMessage = interp.getString(resultObj.items[3]);
        }
      }

      // Reset arenas (parse is always top-level)
      interp.resetScratch();
      wasmInstance.exports.feather_arena_reset();

      return { status, result: resultStr ? `{${resultStr}}` : '', errorMessage };
    },

    eval(interpId, script) {
      const interp = interpreters.get(interpId);
      interp.evalDepth++;
      
      try {
        const [ptr, len] = writeString(script);
        const result = wasmInstance.exports.feather_script_eval(0, interpId, ptr, len, 0);
        // Note: don't free ptr - it's in arena, will be reset
        
        // Capture result BEFORE reset (getString returns plain JS string)
        const resultValue = interp.getString(interp.result);
        
        if (result === TCL_OK) {
          return resultValue;
        }
        // Handle TCL_RETURN at top level - apply the return options
        if (result === TCL_RETURN) {
          // Get return options and apply the code
          let code = TCL_OK;
          const opts = interp.returnOptions.get('current');
          if (opts) {
            const items = interp.getList(opts).items;
            for (let j = 0; j + 1 < items.length; j += 2) {
              const key = interp.getString(items[j]);
              if (key === '-code') {
                const codeStr = interp.getString(items[j + 1]);
                const codeVal = parseInt(codeStr, 10);
                if (!isNaN(codeVal)) {
                  code = codeVal;
                }
              }
            }
          }
          // Apply the extracted code
          if (code === TCL_OK) {
            return resultValue;
          }
          if (code === TCL_ERROR) {
            const error = new Error(resultValue);
            error.code = TCL_ERROR;
            throw error;
          }
          if (code === TCL_BREAK) {
            const error = new Error('invoked "break" outside of a loop');
            error.code = TCL_BREAK;
            throw error;
          }
          if (code === TCL_CONTINUE) {
            const error = new Error('invoked "continue" outside of a loop');
            error.code = TCL_CONTINUE;
            throw error;
          }
          // For other codes, treat as ok
          return resultValue;
        }
        // Convert break/continue outside loop to specific error messages
        if (result === TCL_BREAK) {
          const error = new Error('invoked "break" outside of a loop');
          error.code = result;
          throw error;
        }
        if (result === TCL_CONTINUE) {
          const error = new Error('invoked "continue" outside of a loop');
          error.code = result;
          throw error;
        }
        // For TCL_ERROR and other codes, use the result message
        const error = new Error(resultValue);
        error.code = result;
        throw error;
      } finally {
        interp.evalDepth--;
        
        // Reset arenas only at top-level completion
        if (interp.evalDepth === 0) {
          interp.resetScratch();
          wasmInstance.exports.feather_arena_reset();
        }
      }
    },

    getResult(interpId) {
      const interp = interpreters.get(interpId);
      return interp.getString(interp.result);
    },

    destroy(interpId) {
      interpreters.delete(interpId);
    },

    memoryStats(interpId) {
      const interp = interpreters.get(interpId);
      return {
        scratchHandles: interp.scratch.objects.size,
        wasmArenaUsed: wasmInstance.exports.feather_arena_used(),
        namespaceCount: interp.namespaces.size,
        procCount: interp.procs.size,
        evalDepth: interp.evalDepth,
      };
    },

    forceReset(interpId) {
      const interp = interpreters.get(interpId);
      if (interp.evalDepth > 0) {
        throw new Error('Cannot reset during eval');
      }
      interp.resetScratch();
      wasmInstance.exports.feather_arena_reset();
    },

    get exports() {
      return wasmInstance.exports;
    }
  };
}

function globMatch(pattern, string) {
  let pi = 0, si = 0;
  let starIdx = -1, match = 0;

  while (si < string.length) {
    if (pi < pattern.length && (pattern[pi] === '?' || pattern[pi] === string[si])) {
      pi++;
      si++;
    } else if (pi < pattern.length && pattern[pi] === '*') {
      starIdx = pi++;
      match = si;
    } else if (starIdx !== -1) {
      pi = starIdx + 1;
      si = ++match;
    } else {
      return false;
    }
  }

  while (pi < pattern.length && pattern[pi] === '*') pi++;
  return pi === pattern.length;
}

export { createFeather, TCL_OK, TCL_ERROR, TCL_RETURN, TCL_BREAK, TCL_CONTINUE, TCL_PARSE_OK, TCL_PARSE_INCOMPLETE, TCL_PARSE_ERROR };
