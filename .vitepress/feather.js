/**
 * feather.js - JavaScript host for feather WASM interpreter
 *
 * Works in both Node.js and browsers using ES modules.
 * Provides a complete FeatherHostOps implementation via WASM function tables.
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

const DEFAULT_RECURSION_LIMIT = 1000;

class FeatherInterp {
  constructor(id) {
    this.id = id;
    this.objects = new Map();
    this.nextHandle = 1;
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
  }

  store(obj) {
    const handle = this.nextHandle++;
    this.objects.set(handle, obj);
    return handle;
  }

  invalidateStringCache(obj) {
    if (obj && obj.type === 'list') {
      delete obj.cachedString;
    }
  }

  get(handle) {
    return this.objects.get(handle);
  }

  getString(handle) {
    if (handle === 0) return '';
    const obj = this.get(handle);
    if (!obj) return '';
    if (typeof obj === 'string') return obj;
    if (obj.type === 'string') return obj.value;
    if (obj.type === 'int') return String(obj.value);
    if (obj.type === 'double') return String(obj.value);
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
    if (handle === 0) return [];
    const obj = this.get(handle);
    if (!obj) return [];
    if (obj.type === 'list') return obj.items;
    const str = this.getString(handle);
    return this.parseList(str);
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
    const items = this.getList(handle);
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

  parseList(str) {
    const items = [];
    let i = 0;
    while (i < str.length) {
      while (i < str.length && /\s/.test(str[i])) i++;
      if (i >= str.length) break;

      let word = '';
      if (str[i] === '{') {
        let depth = 1;
        i++;
        const start = i;
        while (i < str.length && depth > 0) {
          if (str[i] === '{') depth++;
          else if (str[i] === '}') depth--;
          if (depth > 0) i++;
        }
        word = str.slice(start, i);
        i++;
      } else if (str[i] === '"') {
        i++;
        const start = i;
        while (i < str.length && str[i] !== '"') {
          if (str[i] === '\\') i++;
          i++;
        }
        word = str.slice(start, i);
        i++;
      } else {
        while (i < str.length && !/\s/.test(str[i])) {
          word += str[i++];
        }
      }
      if (word !== '') items.push(this.store({ type: 'string', value: word }));
    }
    return items;
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
  let wasmTable;
  let wasmInstance;
  let hostOpsPtr = 0; // Set after buildHostOps()

  // Helper to fire variable traces
  const fireVarTraces = (interp, varName, op) => {
    const traces = interp.traces.variable.get(varName);
    if (!traces || traces.length === 0) return;

    for (const trace of traces) {
      // Check if this trace matches the operation
      const ops = trace.ops.split(/\s+/);
      if (!ops.includes(op)) continue;

      // Build the command: script name1 name2 op
      const scriptStr = interp.getString(trace.script);
      const cmd = `${scriptStr} ${varName} {} ${op}`;
      const [ptr, len] = writeString(cmd);
      wasmInstance.exports.feather_script_eval(hostOpsPtr, interp.id, ptr, len, 0);
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
      const scriptStr = interp.getString(trace.script);
      const displayOld = displayName(oldName);
      const displayNew = displayName(newName);
      // Empty strings must be properly quoted with {}
      const quotedNew = displayNew === '' ? '{}' : displayNew;
      const cmd = `${scriptStr} ${displayOld} ${quotedNew} ${op}`;
      const [ptr, len] = writeString(cmd);
      wasmInstance.exports.feather_script_eval(hostOpsPtr, interp.id, ptr, len, 0);
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

  // Check if WebAssembly.Function constructor is available
  // (requires --experimental-wasm-type-reflection in Node.js, or a modern browser with Type Reflection)
  const hasWasmFunction = typeof WebAssembly.Function === 'function';

  // For environments without WebAssembly.Function, we generate trampoline WASM modules
  // Each unique signature needs its own trampoline that imports a JS dispatcher

  // Map from signature key to { module, nextSlot, dispatcher }
  const trampolineCache = new Map();

  // Convert signature to a key string
  const sigKey = (sig) => `${sig.parameters.join(',')}->${sig.results.join(',')}`;

  // WASM binary encoding helpers
  const encodeU32 = (n) => {
    const bytes = [];
    do {
      let byte = n & 0x7f;
      n >>>= 7;
      if (n !== 0) byte |= 0x80;
      bytes.push(byte);
    } while (n !== 0);
    return bytes;
  };

  const encodeI32 = (n) => {
    const bytes = [];
    n |= 0;
    while (true) {
      const byte = n & 0x7f;
      n >>= 7;
      if ((n === 0 && (byte & 0x40) === 0) || (n === -1 && (byte & 0x40) !== 0)) {
        bytes.push(byte);
        break;
      }
      bytes.push(byte | 0x80);
    }
    return bytes;
  };

  const valType = (t) => {
    switch (t) {
      case 'i32': return 0x7f;
      case 'i64': return 0x7e;
      case 'f32': return 0x7d;
      case 'f64': return 0x7c;
      default: throw new Error(`Unknown type: ${t}`);
    }
  };

  // Build a trampoline WASM module for a given signature
  // The module imports a dispatcher function and exports N trampolines
  const buildTrampolineWasm = (sig, count) => {
    const params = sig.parameters;
    const results = sig.results;

    // Build the module bytes
    const bytes = [];

    // Magic + version
    bytes.push(0x00, 0x61, 0x73, 0x6d); // \0asm
    bytes.push(0x01, 0x00, 0x00, 0x00); // version 1

    // Type section (1) - defines the function signature
    const typeSection = [];
    // Function type
    typeSection.push(0x60); // func type
    typeSection.push(params.length);
    params.forEach(p => typeSection.push(valType(p)));
    typeSection.push(results.length);
    results.forEach(r => typeSection.push(valType(r)));

    bytes.push(0x01); // type section
    bytes.push(...encodeU32(typeSection.length + 1));
    bytes.push(0x01); // 1 type
    bytes.push(...typeSection);

    // Import section (2) - import dispatcher functions
    const importSection = [];
    importSection.push(count); // number of imports
    for (let i = 0; i < count; i++) {
      // module name "env"
      importSection.push(0x03, 0x65, 0x6e, 0x76);
      // function name "f0", "f1", etc.
      const fname = `f${i}`;
      importSection.push(fname.length);
      for (let j = 0; j < fname.length; j++) {
        importSection.push(fname.charCodeAt(j));
      }
      // import kind: function, type index 0
      importSection.push(0x00, 0x00);
    }

    bytes.push(0x02); // import section
    bytes.push(...encodeU32(importSection.length));
    bytes.push(...importSection);

    // Function section (3) - declare our trampoline functions
    const funcSection = [count];
    for (let i = 0; i < count; i++) {
      funcSection.push(0x00); // type index 0
    }
    bytes.push(0x03); // function section
    bytes.push(...encodeU32(funcSection.length));
    bytes.push(...funcSection);

    // Export section (7) - export all trampolines
    const exportSection = [count];
    for (let i = 0; i < count; i++) {
      const ename = `t${i}`;
      exportSection.push(ename.length);
      for (let j = 0; j < ename.length; j++) {
        exportSection.push(ename.charCodeAt(j));
      }
      exportSection.push(0x00); // export kind: function
      exportSection.push(...encodeU32(count + i)); // function index (after imports)
    }
    bytes.push(0x07); // export section
    bytes.push(...encodeU32(exportSection.length));
    bytes.push(...exportSection);

    // Code section (10) - function bodies
    const codeSection = [count];
    for (let i = 0; i < count; i++) {
      // Function body: call the imported function with all args
      const body = [];
      body.push(0x00); // no locals

      // Push all parameters
      for (let p = 0; p < params.length; p++) {
        body.push(0x20); // local.get
        body.push(...encodeU32(p));
      }

      // Call imported function
      body.push(0x10); // call
      body.push(...encodeU32(i)); // function index

      body.push(0x0b); // end

      codeSection.push(...encodeU32(body.length));
      codeSection.push(...body);
    }
    bytes.push(0x0a); // code section
    bytes.push(...encodeU32(codeSection.length));
    bytes.push(...codeSection);

    return new Uint8Array(bytes);
  };

  // Registry for trampoline functions per signature
  const trampolineRegistry = new Map(); // sigKey -> { functions: [], instances: [] }

  const TRAMPOLINE_BATCH_SIZE = 64; // Create trampolines in batches

  const getTrampolineFunction = async (fn, signature) => {
    const key = sigKey(signature);
    let reg = trampolineRegistry.get(key);

    if (!reg) {
      reg = { functions: [], instances: [], nextIndex: 0 };
      trampolineRegistry.set(key, reg);
    }

    // Store the JS function
    const fnIndex = reg.functions.length;
    reg.functions.push(fn);

    // Check if we need a new trampoline instance
    const batchIndex = Math.floor(fnIndex / TRAMPOLINE_BATCH_SIZE);
    if (batchIndex >= reg.instances.length) {
      // Build a new batch of trampolines
      const wasmBytes = buildTrampolineWasm(signature, TRAMPOLINE_BATCH_SIZE);
      const module = await WebAssembly.compile(wasmBytes);

      // Create imports object with dispatcher functions
      const imports = { env: {} };
      for (let i = 0; i < TRAMPOLINE_BATCH_SIZE; i++) {
        const globalIdx = batchIndex * TRAMPOLINE_BATCH_SIZE + i;
        imports.env[`f${i}`] = (...args) => {
          const f = reg.functions[globalIdx];
          return f ? f(...args) : (signature.results.length > 0 ? 0 : undefined);
        };
      }

      const instance = await WebAssembly.instantiate(module, imports);
      reg.instances.push(instance);
    }

    // Get the trampoline function from the appropriate instance
    const localIndex = fnIndex % TRAMPOLINE_BATCH_SIZE;
    const instance = reg.instances[batchIndex];
    return instance.exports[`t${localIndex}`];
  };

  const addToTable = async (fn, signature) => {
    if (hasWasmFunction) {
      const wasmFn = new WebAssembly.Function(signature, fn);
      const index = wasmTable.length;
      wasmTable.grow(1);
      wasmTable.set(index, wasmFn);
      return index;
    } else {
      // Use trampoline approach for browsers
      const trampolineFn = await getTrampolineFunction(fn, signature);
      const index = wasmTable.length;
      wasmTable.grow(1);
      wasmTable.set(index, trampolineFn);
      return index;
    }
  };

  const hostFunctions = {
    // Frame operations
    frame_push: (interpId, cmd, args) => {
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
    frame_pop: (interpId) => {
      const interp = interpreters.get(interpId);
      if (interp.frames.length > 1) {
        interp.frames.pop();
        interp.activeLevel = Math.min(interp.activeLevel, interp.frames.length - 1);
      }
      return TCL_OK;
    },
    frame_level: (interpId) => {
      return interpreters.get(interpId).activeLevel;
    },
    frame_set_active: (interpId, level) => {
      const interp = interpreters.get(interpId);
      if (level >= interp.frames.length) return TCL_ERROR;
      interp.activeLevel = level;
      return TCL_OK;
    },
    frame_size: (interpId) => {
      return interpreters.get(interpId).frames.length;
    },
    frame_info: (interpId, level, cmdPtr, argsPtr, nsPtr) => {
      const interp = interpreters.get(interpId);
      if (level >= interp.frames.length) return TCL_ERROR;
      const frame = interp.frames[level];
      writeI32(cmdPtr, frame.cmd);
      writeI32(argsPtr, frame.args);
      const nsHandle = interp.store({ type: 'string', value: frame.ns });
      writeI32(nsPtr, nsHandle);
      return TCL_OK;
    },
    frame_set_namespace: (interpId, ns) => {
      const interp = interpreters.get(interpId);
      interp.currentFrame().ns = interp.getString(ns);
      return TCL_OK;
    },
    frame_get_namespace: (interpId) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'string', value: interp.currentFrame().ns });
    },

    // Variable operations
    var_get: (interpId, name) => {
      const interp = interpreters.get(interpId);
      const varName = interp.getString(name);
      const frame = interp.currentFrame();
      const entry = frame.vars.get(varName);
      if (!entry) return 0;
      let result;
      if (entry.link) {
        const targetFrame = interp.frames[entry.link.level];
        const targetEntry = targetFrame?.vars.get(entry.link.name);
        if (!targetEntry) return 0;
        result = typeof targetEntry === 'object' && 'value' in targetEntry ? targetEntry.value : targetEntry;
      } else if (entry.nsLink) {
        const ns = interp.getNamespace(entry.nsLink.ns);
        const nsEntry = ns?.vars.get(entry.nsLink.name);
        if (!nsEntry) return 0;
        result = typeof nsEntry === 'object' && 'value' in nsEntry ? nsEntry.value : nsEntry;
      } else {
        result = entry.value || 0;
      }
      // Fire read traces
      fireVarTraces(interp, varName, 'read');
      return result;
    },
    var_set: (interpId, name, value) => {
      const interp = interpreters.get(interpId);
      const varName = interp.getString(name);
      const frame = interp.currentFrame();
      const entry = frame.vars.get(varName);
      if (entry?.link) {
        const targetFrame = interp.frames[entry.link.level];
        if (targetFrame) {
          let targetEntry = targetFrame.vars.get(entry.link.name);
          if (!targetEntry) targetEntry = {};
          targetEntry.value = value;
          targetFrame.vars.set(entry.link.name, targetEntry);
        }
      } else if (entry?.nsLink) {
        const ns = interp.getNamespace(entry.nsLink.ns);
        if (ns) ns.vars.set(entry.nsLink.name, value);
      } else {
        frame.vars.set(varName, { value });
      }
      // Fire write traces
      fireVarTraces(interp, varName, 'write');
    },
    var_unset: (interpId, name) => {
      const interp = interpreters.get(interpId);
      const varName = interp.getString(name);
      interp.currentFrame().vars.delete(varName);
      // Fire unset traces
      fireVarTraces(interp, varName, 'unset');
    },
    var_exists: (interpId, name) => {
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
    var_link: (interpId, local, targetLevel, target) => {
      const interp = interpreters.get(interpId);
      const localName = interp.getString(local);
      const targetName = interp.getString(target);
      interp.currentFrame().vars.set(localName, { link: { level: targetLevel, name: targetName } });
    },
    var_link_ns: (interpId, local, ns, name) => {
      const interp = interpreters.get(interpId);
      const localName = interp.getString(local);
      const nsPath = interp.getString(ns);
      const varName = interp.getString(name);
      interp.ensureNamespace(nsPath);
      interp.currentFrame().vars.set(localName, { nsLink: { ns: nsPath, name: varName } });
    },
    var_names: (interpId, ns) => {
      const interp = interpreters.get(interpId);
      let names;
      if (ns === 0) {
        names = [...interp.currentFrame().vars.keys()];
      } else {
        const nsPath = interp.getString(ns);
        const namespace = interp.getNamespace(nsPath);
        names = namespace ? [...namespace.vars.keys()] : [];
      }
      // Sort for consistent ordering (matches Go implementation)
      names.sort();
      const list = { type: 'list', items: names.map(n => interp.store({ type: 'string', value: n })) };
      return interp.store(list);
    },

    // Proc operations
    proc_define: (interpId, name, params, body) => {
      const interp = interpreters.get(interpId);
      const procName = interp.getString(name);
      interp.procs.set(procName, { params, body });

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
      namespace.commands.set(simpleName, { kind: TCL_CMD_PROC, fn: 0, params, body });
    },
    proc_exists: (interpId, name) => {
      const interp = interpreters.get(interpId);
      const procName = interp.getString(name);
      return interp.procs.has(procName) ? 1 : 0;
    },
    proc_params: (interpId, name, resultPtr) => {
      const interp = interpreters.get(interpId);
      const procName = interp.getString(name);
      // Try both qualified and simple names
      const proc = interp.procs.get(procName) || interp.procs.get(`::${procName}`);
      if (proc) {
        writeI32(resultPtr, proc.params);
        return TCL_OK;
      }
      return TCL_ERROR;
    },
    proc_body: (interpId, name, resultPtr) => {
      const interp = interpreters.get(interpId);
      const procName = interp.getString(name);
      // Try both qualified and simple names
      const proc = interp.procs.get(procName) || interp.procs.get(`::${procName}`);
      if (proc) {
        writeI32(resultPtr, proc.body);
        return TCL_OK;
      }
      return TCL_ERROR;
    },
    proc_names: (interpId, ns) => {
      const interp = interpreters.get(interpId);
      const names = [...interp.procs.keys(), ...interp.builtins.keys()];
      const list = { type: 'list', items: names.map(n => interp.store({ type: 'string', value: n })) };
      return interp.store(list);
    },
    proc_resolve_namespace: (interpId, path, resultPtr) => {
      const interp = interpreters.get(interpId);
      writeI32(resultPtr, path || interp.store({ type: 'string', value: '::' }));
      return TCL_OK;
    },
    proc_register_builtin: (interpId, name, fn) => {
      const interp = interpreters.get(interpId);
      const procName = interp.getString(name);
      interp.builtins.set(procName, fn);
    },
    proc_lookup: (interpId, name, fnPtr) => {
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
    proc_rename: (interpId, oldName, newName) => {
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
      let isBuiltin = false;
      if (!cmd && interp.procs.has(oldN)) {
        const proc = interp.procs.get(oldN);
        cmd = { kind: TCL_CMD_PROC, fn: 0, params: proc.params, body: proc.body };
      } else if (!cmd && interp.builtins.has(oldN)) {
        cmd = { kind: TCL_CMD_BUILTIN, fn: interp.builtins.get(oldN) };
        isBuiltin = true;
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

    // String operations
    string_intern: (interpId, ptr, len) => {
      const interp = interpreters.get(interpId);
      const str = readString(ptr, len);
      return interp.store({ type: 'string', value: str });
    },
    string_get: (interpId, handle, lenPtr) => {
      const interp = interpreters.get(interpId);
      const str = interp.getString(handle);
      const [ptr, len] = writeString(str);
      writeI32(lenPtr, len);
      return ptr;
    },
    string_concat: (interpId, a, b) => {
      const interp = interpreters.get(interpId);
      const strA = interp.getString(a);
      const strB = interp.getString(b);
      return interp.store({ type: 'string', value: strA + strB });
    },
    string_compare: (interpId, a, b) => {
      const interp = interpreters.get(interpId);
      const strA = interp.getString(a);
      const strB = interp.getString(b);
      return strA < strB ? -1 : strA > strB ? 1 : 0;
    },
    string_regex_match: (interpId, pattern, string, resultPtr) => {
      const interp = interpreters.get(interpId);
      const patternStr = interp.getString(pattern);
      const stringStr = interp.getString(string);
      try {
        const regex = new RegExp(patternStr);
        writeI32(resultPtr, regex.test(stringStr) ? 1 : 0);
        return TCL_OK;
      } catch (e) {
        interp.result = interp.store({ type: 'string', value: `invalid regex: ${e.message}` });
        return TCL_ERROR;
      }
    },

    // Rune operations (Unicode-aware)
    rune_length: (interpId, str) => {
      const interp = interpreters.get(interpId);
      const s = interp.getString(str);
      return [...s].length;
    },
    rune_at: (interpId, str, index) => {
      const interp = interpreters.get(interpId);
      const s = interp.getString(str);
      const chars = [...s];
      if (index >= chars.length) return interp.store({ type: 'string', value: '' });
      return interp.store({ type: 'string', value: chars[index] });
    },
    rune_range: (interpId, str, first, last) => {
      const interp = interpreters.get(interpId);
      const s = interp.getString(str);
      const chars = [...s];
      const f = Math.max(0, Number(first));
      const l = Math.min(chars.length - 1, Number(last));
      if (f > l) return interp.store({ type: 'string', value: '' });
      return interp.store({ type: 'string', value: chars.slice(f, l + 1).join('') });
    },
    rune_to_upper: (interpId, str) => {
      const interp = interpreters.get(interpId);
      const s = interp.getString(str);
      return interp.store({ type: 'string', value: s.toUpperCase() });
    },
    rune_to_lower: (interpId, str) => {
      const interp = interpreters.get(interpId);
      const s = interp.getString(str);
      return interp.store({ type: 'string', value: s.toLowerCase() });
    },
    rune_fold: (interpId, str) => {
      const interp = interpreters.get(interpId);
      const s = interp.getString(str);
      return interp.store({ type: 'string', value: s.toLowerCase() });
    },

    // Namespace operations
    ns_create: (interpId, path) => {
      const interp = interpreters.get(interpId);
      interp.ensureNamespace(interp.getString(path));
      return TCL_OK;
    },
    ns_delete: (interpId, path) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(path).replace(/^::/, '');
      if (nsPath === '') return TCL_ERROR; // Cannot delete global namespace
      const namespace = interp.namespaces.get(nsPath);
      if (!namespace) return TCL_ERROR;

      // Delete all children recursively
      const deleteRecursive = (ns) => {
        for (const childPath of ns.children.values()) {
          const child = interp.namespaces.get(childPath);
          if (child) deleteRecursive(child);
          interp.namespaces.delete(childPath);
        }
      };
      deleteRecursive(namespace);
      interp.namespaces.delete(nsPath);

      // Remove from parent's children
      const parts = nsPath.split('::').filter(p => p);
      if (parts.length > 1) {
        const parentPath = parts.slice(0, -1).join('::');
        const parent = interp.namespaces.get(parentPath);
        if (parent) {
          const childName = parts[parts.length - 1];
          parent.children.delete(childName);
        }
      } else {
        // Direct child of global namespace
        const global = interp.namespaces.get('');
        if (global) global.children.delete(nsPath);
      }
      return TCL_OK;
    },
    ns_exists: (interpId, path) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(path).replace(/^::/, '');
      return interp.namespaces.has(nsPath) ? 1 : 0;
    },
    ns_current: (interpId) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'string', value: interp.currentFrame().ns });
    },
    ns_parent: (interpId, ns, resultPtr) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns).replace(/^::/, '');
      // Global namespace has no parent - return empty string
      if (nsPath === '') {
        writeI32(resultPtr, interp.store({ type: 'string', value: '' }));
        return TCL_OK;
      }
      const parts = nsPath.split('::').filter(p => p);
      parts.pop();
      const parent = parts.length ? '::' + parts.join('::') : '::';
      writeI32(resultPtr, interp.store({ type: 'string', value: parent }));
      return TCL_OK;
    },
    ns_children: (interpId, ns) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns).replace(/^::/, '');
      const namespace = interp.namespaces.get(nsPath);
      if (!namespace) return interp.store({ type: 'list', items: [] });
      // Sort children alphabetically for consistent ordering (like Go)
      const sortedNames = [...namespace.children.keys()].sort();
      const children = sortedNames.map(name => {
        const childPath = namespace.children.get(name);
        return interp.store({ type: 'string', value: '::' + childPath });
      });
      return interp.store({ type: 'list', items: children });
    },
    ns_get_var: (interpId, ns, name) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns);
      const varName = interp.getString(name);
      const namespace = interp.getNamespace(nsPath);
      const entry = namespace?.vars.get(varName);
      // Support both { value: handle } format (from var_set) and raw handle (from ns_set_var)
      if (!entry) return 0;
      return typeof entry === 'object' && 'value' in entry ? entry.value : entry;
    },
    ns_set_var: (interpId, ns, name, value) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns);
      const varName = interp.getString(name);
      const namespace = interp.ensureNamespace(nsPath);
      // Use { value } wrapper for consistency with var_set
      namespace.vars.set(varName, { value });
    },
    ns_var_exists: (interpId, ns, name) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns);
      const varName = interp.getString(name);
      const namespace = interp.getNamespace(nsPath);
      return namespace?.vars.has(varName) ? 1 : 0;
    },
    ns_unset_var: (interpId, ns, name) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns);
      const varName = interp.getString(name);
      const namespace = interp.getNamespace(nsPath);
      if (namespace) namespace.vars.delete(varName);
    },
    ns_get_command: (interpId, ns, name, fnPtr) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns).replace(/^::/, '');
      const cmdName = interp.getString(name);
      const namespace = interp.namespaces.get(nsPath);

      // Check namespace-local commands first
      if (namespace?.commands.has(cmdName)) {
        const cmd = namespace.commands.get(cmdName);
        writeI32(fnPtr, cmd.fn || 0);
        return cmd.kind;
      }
      // Fallback to global builtins for backwards compatibility
      if (interp.builtins.has(cmdName)) {
        writeI32(fnPtr, interp.builtins.get(cmdName));
        return TCL_CMD_BUILTIN;
      }
      writeI32(fnPtr, 0);
      return TCL_CMD_NONE;
    },
    ns_set_command: (interpId, ns, name, kind, fn, params, body) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns).replace(/^::/, '');
      const cmdName = interp.getString(name);
      const namespace = interp.ensureNamespace('::' + nsPath);

      // Store command in namespace
      namespace.commands.set(cmdName, { kind, fn, params, body });

      // Also store in legacy maps for backwards compatibility
      if (kind === TCL_CMD_BUILTIN) {
        interp.builtins.set(cmdName, fn);
      } else if (kind === TCL_CMD_PROC) {
        const fullName = nsPath === '' ? `::${cmdName}` : `::${nsPath}::${cmdName}`;
        interp.procs.set(fullName, { params, body });
      }
    },
    ns_delete_command: (interpId, ns, name) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns).replace(/^::/, '');
      const cmdName = interp.getString(name);
      const namespace = interp.namespaces.get(nsPath);

      if (namespace?.commands.delete(cmdName)) {
        // Also clean up legacy maps
        interp.builtins.delete(cmdName);
        const fullName = nsPath === '' ? `::${cmdName}` : `::${nsPath}::${cmdName}`;
        interp.procs.delete(fullName);
        return TCL_OK;
      }
      // Try legacy maps as fallback
      if (interp.procs.delete(cmdName) || interp.builtins.delete(cmdName)) {
        return TCL_OK;
      }
      return TCL_ERROR;
    },
    ns_list_commands: (interpId, ns) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns).replace(/^::/, '');
      const namespace = interp.namespaces.get(nsPath);

      if (!namespace) {
        return interp.store({ type: 'list', items: [] });
      }
      // Get commands from namespace and sort alphabetically
      const names = [...namespace.commands.keys()].sort();
      return interp.store({ type: 'list', items: names.map(n => interp.store({ type: 'string', value: n })) });
    },
    ns_get_exports: (interpId, ns) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns).replace(/^::/, '');
      const namespace = interp.namespaces.get(nsPath);
      const exports = namespace?.exports || [];
      return interp.store({ type: 'list', items: exports.map(e => interp.store({ type: 'string', value: e })) });
    },
    ns_set_exports: (interpId, ns, patterns, clear) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns).replace(/^::/, '');
      const namespace = interp.ensureNamespace(nsPath);
      const patternList = interp.getList(patterns).map(h => interp.getString(h));
      if (clear) {
        namespace.exports = patternList;
      } else {
        namespace.exports.push(...patternList);
      }
    },
    ns_is_exported: (interpId, ns, name) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns).replace(/^::/, '');
      const cmdName = interp.getString(name);
      const namespace = interp.namespaces.get(nsPath);
      if (!namespace) return 0;
      return namespace.exports.some(pattern => globMatch(pattern, cmdName)) ? 1 : 0;
    },
    ns_copy_command: (interpId, srcNs, srcName, dstNs, dstName) => {
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
      // Fallback to legacy maps
      if (interp.builtins.has(src)) {
        interp.builtins.set(dst, interp.builtins.get(src));
        dstNamespace.commands.set(dst, { kind: TCL_CMD_BUILTIN, fn: interp.builtins.get(src) });
        return TCL_OK;
      }
      return TCL_ERROR;
    },

    // List operations
    list_is_nil: (interpId, obj) => obj === 0 ? 1 : 0,
    list_create: (interpId) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'list', items: [] });
    },
    list_from: (interpId, obj) => {
      const interp = interpreters.get(interpId);
      const items = interp.getList(obj);
      return interp.store({ type: 'list', items: [...items] });
    },
    list_push: (interpId, list, item) => {
      const interp = interpreters.get(interpId);
      const listObj = interp.get(list);
      if (listObj?.type === 'list') {
        listObj.items.push(item);
        interp.invalidateStringCache(listObj);
      }
      return list;
    },
    list_pop: (interpId, list) => {
      const interp = interpreters.get(interpId);
      const listObj = interp.get(list);
      if (listObj?.type === 'list' && listObj.items.length > 0) {
        interp.invalidateStringCache(listObj);
        return listObj.items.pop();
      }
      return 0;
    },
    list_unshift: (interpId, list, item) => {
      const interp = interpreters.get(interpId);
      const listObj = interp.get(list);
      if (listObj?.type === 'list') {
        listObj.items.unshift(item);
        interp.invalidateStringCache(listObj);
      }
      return list;
    },
    list_shift: (interpId, list) => {
      const interp = interpreters.get(interpId);
      const listObj = interp.get(list);
      if (listObj?.type === 'list' && listObj.items.length > 0) {
        interp.invalidateStringCache(listObj);
        return listObj.items.shift();
      }
      return 0;
    },
    list_length: (interpId, list) => {
      const interp = interpreters.get(interpId);
      // Use getList for shimmering (string → list)
      const items = interp.getList(list);
      return items.length;
    },
    list_at: (interpId, list, index) => {
      const interp = interpreters.get(interpId);
      // Use getList for shimmering (string → list)
      const items = interp.getList(list);
      if (index < items.length) {
        return items[index];
      }
      return 0;
    },
    list_slice: (interpId, list, first, last) => {
      const interp = interpreters.get(interpId);
      // Use getList for shimmering (string → list)
      const items = interp.getList(list);
      const sliced = items.slice(Number(first), Number(last) + 1);
      return interp.store({ type: 'list', items: sliced });
    },
    list_set_at: (interpId, list, index, value) => {
      const interp = interpreters.get(interpId);
      const listObj = interp.get(list);
      // For set_at, we need a native list - can't shimmer and modify
      if (listObj?.type === 'list' && index < listObj.items.length) {
        listObj.items[index] = value;
        interp.invalidateStringCache(listObj);
        return TCL_OK;
      }
      return TCL_ERROR;
    },
    list_splice: (interpId, list, first, deleteCount, insertions) => {
      const interp = interpreters.get(interpId);
      const listObj = interp.get(list);
      // For splice, we need a native list - can't shimmer and modify
      if (listObj?.type === 'list') {
        const toInsert = interp.getList(insertions);
        listObj.items.splice(Number(first), Number(deleteCount), ...toInsert);
        interp.invalidateStringCache(listObj);
      }
      return list;
    },
    list_sort: (interpId, list, cmpFn, ctx) => {
      const interp = interpreters.get(interpId);
      const listObj = interp.get(list);
      if (listObj?.type !== 'list' || listObj.items.length <= 1) {
        return TCL_OK;
      }
      // Get the comparison function from the WASM table
      const compareFn = wasmTable.get(cmpFn);
      // Sort using JavaScript's sort with the WASM comparison function
      listObj.items.sort((a, b) => {
        return compareFn(interpId, a, b, ctx);
      });
      // Invalidate string cache since list order changed
      interp.invalidateStringCache(listObj);
      return TCL_OK;
    },

    // Dict operations
    dict_create: (interpId) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'dict', entries: [] });
    },
    dict_is_dict: (interpId, obj) => {
      const interp = interpreters.get(interpId);
      const o = interp.get(obj);
      return o?.type === 'dict' ? 1 : 0;
    },
    dict_from: (interpId, obj) => {
      const interp = interpreters.get(interpId);
      const items = interp.getList(obj);
      if (items.length % 2 !== 0) return 0;
      const entries = [];
      for (let i = 0; i < items.length; i += 2) {
        entries.push([items[i], items[i + 1]]);
      }
      return interp.store({ type: 'dict', entries });
    },
    dict_get: (interpId, dict, key) => {
      const interp = interpreters.get(interpId);
      const dictData = interp.getDict(dict);
      if (!dictData) return 0;
      const keyStr = interp.getString(key);
      for (const [k, v] of dictData.entries) {
        if (interp.getString(k) === keyStr) return v;
      }
      return 0;
    },
    dict_set: (interpId, dict, key, value) => {
      const interp = interpreters.get(interpId);
      // Shimmer to dict first
      const dictData = interp.getDict(dict);
      const dictObj = interp.get(dict);
      if (!dictObj || dictObj.type !== 'dict') {
        // Create new dict if shimmering failed
        const newDict = { type: 'dict', entries: [[key, value]] };
        return interp.store(newDict);
      }
      const keyStr = interp.getString(key);
      for (let i = 0; i < dictObj.entries.length; i++) {
        if (interp.getString(dictObj.entries[i][0]) === keyStr) {
          dictObj.entries[i][1] = value;
          return dict;
        }
      }
      dictObj.entries.push([key, value]);
      return dict;
    },
    dict_exists: (interpId, dict, key) => {
      const interp = interpreters.get(interpId);
      const dictData = interp.getDict(dict);
      if (!dictData) return 0;
      const keyStr = interp.getString(key);
      return dictData.entries.some(([k]) => interp.getString(k) === keyStr) ? 1 : 0;
    },
    dict_remove: (interpId, dict, key) => {
      const interp = interpreters.get(interpId);
      const dictData = interp.getDict(dict);
      if (!dictData) return dict;
      const dictObj = interp.get(dict);
      const keyStr = interp.getString(key);
      dictObj.entries = dictObj.entries.filter(([k]) => interp.getString(k) !== keyStr);
      return dict;
    },
    dict_size: (interpId, dict) => {
      const interp = interpreters.get(interpId);
      const dictData = interp.getDict(dict);
      return dictData ? dictData.entries.length : 0;
    },
    dict_keys: (interpId, dict) => {
      const interp = interpreters.get(interpId);
      const dictData = interp.getDict(dict);
      if (!dictData) return interp.store({ type: 'list', items: [] });
      return interp.store({ type: 'list', items: dictData.entries.map(([k]) => k) });
    },
    dict_values: (interpId, dict) => {
      const interp = interpreters.get(interpId);
      const dictData = interp.getDict(dict);
      if (!dictData) return interp.store({ type: 'list', items: [] });
      return interp.store({ type: 'list', items: dictData.entries.map(([, v]) => v) });
    },

    // Integer operations
    int_create: (interpId, value) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'int', value: Number(value) });
    },
    int_get: (interpId, handle, outPtr) => {
      const interp = interpreters.get(interpId);
      const obj = interp.get(handle);
      if (obj?.type === 'int') {
        writeI64(outPtr, obj.value);
        return TCL_OK;
      }
      if (obj?.type === 'double') {
        // Shimmer from double
        writeI64(outPtr, Math.trunc(obj.value));
        return TCL_OK;
      }
      const str = interp.getString(handle);
      // Strict integer parsing - reject floats and non-numeric strings
      // Only accept optional leading +/-, digits, and whitespace
      if (!/^\s*[+-]?\d+\s*$/.test(str)) {
        return TCL_ERROR;
      }
      const num = parseInt(str, 10);
      if (!isNaN(num)) {
        writeI64(outPtr, num);
        return TCL_OK;
      }
      return TCL_ERROR;
    },

    // Double operations
    dbl_create: (interpId, value) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'double', value });
    },
    dbl_get: (interpId, handle, outPtr) => {
      const interp = interpreters.get(interpId);
      const obj = interp.get(handle);
      if (obj?.type === 'double') {
        writeF64(outPtr, obj.value);
        return TCL_OK;
      }
      if (obj?.type === 'int') {
        // Shimmer from int
        writeF64(outPtr, obj.value);
        return TCL_OK;
      }
      const str = interp.getString(handle).trim();
      // Strict float parsing - match Go's strconv.ParseFloat behavior
      // Reject hex strings (0x...), octal prefixes that don't parse as float
      if (str === '') return TCL_ERROR;
      // Reject hex notation - Go's ParseFloat doesn't accept it
      if (/^[+-]?0[xX]/.test(str)) return TCL_ERROR;
      const num = Number(str);
      if (!isNaN(num) && isFinite(num)) {
        writeF64(outPtr, num);
        return TCL_OK;
      }
      return TCL_ERROR;
    },

    // Interpreter operations
    interp_set_result: (interpId, result) => {
      interpreters.get(interpId).result = result;
      return TCL_OK;
    },
    interp_get_result: (interpId) => {
      return interpreters.get(interpId).result;
    },
    interp_reset_result: (interpId, result) => {
      const interp = interpreters.get(interpId);
      interp.result = result;
      interp.returnOptions.clear();
      return TCL_OK;
    },
    interp_set_return_options: (interpId, options) => {
      const interp = interpreters.get(interpId);
      interp.returnOptions.set('current', options);
      return TCL_OK;
    },
    interp_get_return_options: (interpId, code) => {
      const interp = interpreters.get(interpId);
      const opts = interp.returnOptions.get('current');
      return opts || 0;
    },
    interp_get_script: (interpId) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'string', value: interp.scriptPath });
    },
    interp_set_script: (interpId, path) => {
      const interp = interpreters.get(interpId);
      interp.scriptPath = interp.getString(path);
    },

    // Bind operations
    bind_unknown: (interpId, cmd, args, valuePtr) => {
      const interp = interpreters.get(interpId);
      const cmdName = interp.getString(cmd);
      const hostFn = interp.hostCommands.get(cmdName);
      if (!hostFn) {
        interp.result = interp.store({ type: 'string', value: `invalid command name "${cmdName}"` });
        return TCL_ERROR;
      }

      const argList = interp.getList(args).map(h => interp.getString(h));
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
    trace_add: (interpId, kind, name, ops, script) => {
      const interp = interpreters.get(interpId);
      const kindStr = interp.getString(kind);
      const nameStr = interp.getString(name);
      const opsStr = interp.getString(ops);
      const traces = interp.traces[kindStr];
      if (!traces) return TCL_ERROR;
      if (!traces.has(nameStr)) traces.set(nameStr, []);
      traces.get(nameStr).push({ ops: opsStr, script });
      return TCL_OK;
    },
    trace_remove: (interpId, kind, name, ops, script) => {
      const interp = interpreters.get(interpId);
      const kindStr = interp.getString(kind);
      const nameStr = interp.getString(name);
      const opsStr = interp.getString(ops);
      const traces = interp.traces[kindStr]?.get(nameStr);
      if (!traces) return TCL_ERROR;
      const scriptStr = interp.getString(script);
      const idx = traces.findIndex(t => t.ops === opsStr && interp.getString(t.script) === scriptStr);
      if (idx >= 0) {
        traces.splice(idx, 1);
        return TCL_OK;
      }
      return TCL_ERROR;
    },
    trace_info: (interpId, kind, name) => {
      const interp = interpreters.get(interpId);
      const kindStr = interp.getString(kind);
      const nameStr = interp.getString(name);
      const traces = interp.traces[kindStr]?.get(nameStr) || [];
      const items = traces.map(t => {
        // Split ops into individual elements, then append script
        const ops = t.ops.split(/\s+/).filter(o => o);
        const subItems = ops.map(op => interp.store({ type: 'string', value: op }));
        subItems.push(t.script);
        return interp.store({ type: 'list', items: subItems });
      });
      return interp.store({ type: 'list', items });
    },

    // Foreign object operations
    foreign_is_foreign: (interpId, obj) => {
      const interp = interpreters.get(interpId);
      return interp.isForeign(obj) ? 1 : 0;
    },
    foreign_type_name: (interpId, obj) => {
      const interp = interpreters.get(interpId);
      const typeName = interp.getForeignTypeName(obj);
      if (!typeName) return 0;
      return interp.store({ type: 'string', value: typeName });
    },
    foreign_string_rep: (interpId, obj) => {
      const interp = interpreters.get(interpId);
      const instance = interp.getForeignInstance(obj);
      if (!instance) return 0;
      return interp.store({ type: 'string', value: instance.handleName || `<${instance.typeName}:${obj}>` });
    },
    foreign_methods: (interpId, obj) => {
      const interp = interpreters.get(interpId);
      const typeName = interp.getForeignTypeName(obj);
      if (!typeName) return interp.store({ type: 'list', items: [] });
      const typeDef = interp.foreignTypes.get(typeName);
      if (!typeDef) return interp.store({ type: 'list', items: [] });
      const methods = Object.keys(typeDef.methods || {}).concat(['destroy']);
      return interp.store({ type: 'list', items: methods.map(m => interp.store({ type: 'string', value: m })) });
    },
    foreign_invoke: (interpId, obj, method, args) => {
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
        const argList = interp.getList(args).map(h => interp.getString(h));
        const result = fn(o.value, ...argList);
        interp.result = interp.store({ type: 'string', value: String(result ?? '') });
        return TCL_OK;
      } catch (e) {
        interp.result = interp.store({ type: 'string', value: e.message });
        return TCL_ERROR;
      }
    },
    foreign_destroy: (interpId, obj) => {
      const interp = interpreters.get(interpId);
      const o = interp.get(obj);
      if (o?.type !== 'foreign') return;
      const typeDef = interp.foreignTypes.get(o.typeName);
      typeDef?.destroy?.(o.value);
    },
  };

  const signatures = {
    frame_push: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    frame_pop: { parameters: ['i32'], results: ['i32'] },
    frame_level: { parameters: ['i32'], results: ['i32'] },
    frame_set_active: { parameters: ['i32', 'i32'], results: ['i32'] },
    frame_size: { parameters: ['i32'], results: ['i32'] },
    frame_info: { parameters: ['i32', 'i32', 'i32', 'i32', 'i32'], results: ['i32'] },
    frame_set_namespace: { parameters: ['i32', 'i32'], results: ['i32'] },
    frame_get_namespace: { parameters: ['i32'], results: ['i32'] },

    var_get: { parameters: ['i32', 'i32'], results: ['i32'] },
    var_set: { parameters: ['i32', 'i32', 'i32'], results: [] },
    var_unset: { parameters: ['i32', 'i32'], results: [] },
    var_exists: { parameters: ['i32', 'i32'], results: ['i32'] },
    var_link: { parameters: ['i32', 'i32', 'i32', 'i32'], results: [] },
    var_link_ns: { parameters: ['i32', 'i32', 'i32', 'i32'], results: [] },
    var_names: { parameters: ['i32', 'i32'], results: ['i32'] },

    proc_define: { parameters: ['i32', 'i32', 'i32', 'i32'], results: [] },
    proc_exists: { parameters: ['i32', 'i32'], results: ['i32'] },
    proc_params: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    proc_body: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    proc_names: { parameters: ['i32', 'i32'], results: ['i32'] },
    proc_resolve_namespace: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    proc_register_builtin: { parameters: ['i32', 'i32', 'i32'], results: [] },
    proc_lookup: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    proc_rename: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },

    ns_create: { parameters: ['i32', 'i32'], results: ['i32'] },
    ns_delete: { parameters: ['i32', 'i32'], results: ['i32'] },
    ns_exists: { parameters: ['i32', 'i32'], results: ['i32'] },
    ns_current: { parameters: ['i32'], results: ['i32'] },
    ns_parent: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    ns_children: { parameters: ['i32', 'i32'], results: ['i32'] },
    ns_get_var: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    ns_set_var: { parameters: ['i32', 'i32', 'i32', 'i32'], results: [] },
    ns_var_exists: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    ns_unset_var: { parameters: ['i32', 'i32', 'i32'], results: [] },
    ns_get_command: { parameters: ['i32', 'i32', 'i32', 'i32'], results: ['i32'] },
    ns_set_command: { parameters: ['i32', 'i32', 'i32', 'i32', 'i32', 'i32', 'i32'], results: [] },
    ns_delete_command: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    ns_list_commands: { parameters: ['i32', 'i32'], results: ['i32'] },
    ns_get_exports: { parameters: ['i32', 'i32'], results: ['i32'] },
    ns_set_exports: { parameters: ['i32', 'i32', 'i32', 'i32'], results: [] },
    ns_is_exported: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    ns_copy_command: { parameters: ['i32', 'i32', 'i32', 'i32', 'i32'], results: ['i32'] },

    string_intern: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    string_get: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    string_concat: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    string_compare: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    string_regex_match: { parameters: ['i32', 'i32', 'i32', 'i32'], results: ['i32'] },

    rune_length: { parameters: ['i32', 'i32'], results: ['i32'] },
    rune_at: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    rune_range: { parameters: ['i32', 'i32', 'i64', 'i64'], results: ['i32'] },
    rune_to_upper: { parameters: ['i32', 'i32'], results: ['i32'] },
    rune_to_lower: { parameters: ['i32', 'i32'], results: ['i32'] },
    rune_fold: { parameters: ['i32', 'i32'], results: ['i32'] },

    list_is_nil: { parameters: ['i32', 'i32'], results: ['i32'] },
    list_create: { parameters: ['i32'], results: ['i32'] },
    list_from: { parameters: ['i32', 'i32'], results: ['i32'] },
    list_push: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    list_pop: { parameters: ['i32', 'i32'], results: ['i32'] },
    list_unshift: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    list_shift: { parameters: ['i32', 'i32'], results: ['i32'] },
    list_length: { parameters: ['i32', 'i32'], results: ['i32'] },
    list_at: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    list_slice: { parameters: ['i32', 'i32', 'i32', 'i32'], results: ['i32'] },
    list_set_at: { parameters: ['i32', 'i32', 'i32', 'i32'], results: ['i32'] },
    list_splice: { parameters: ['i32', 'i32', 'i32', 'i32', 'i32'], results: ['i32'] },
    list_sort: { parameters: ['i32', 'i32', 'i32', 'i32'], results: ['i32'] },

    dict_create: { parameters: ['i32'], results: ['i32'] },
    dict_is_dict: { parameters: ['i32', 'i32'], results: ['i32'] },
    dict_from: { parameters: ['i32', 'i32'], results: ['i32'] },
    dict_get: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    dict_set: { parameters: ['i32', 'i32', 'i32', 'i32'], results: ['i32'] },
    dict_exists: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    dict_remove: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },
    dict_size: { parameters: ['i32', 'i32'], results: ['i32'] },
    dict_keys: { parameters: ['i32', 'i32'], results: ['i32'] },
    dict_values: { parameters: ['i32', 'i32'], results: ['i32'] },

    int_create: { parameters: ['i32', 'i64'], results: ['i32'] },
    int_get: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },

    dbl_create: { parameters: ['i32', 'f64'], results: ['i32'] },
    dbl_get: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },

    interp_set_result: { parameters: ['i32', 'i32'], results: ['i32'] },
    interp_get_result: { parameters: ['i32'], results: ['i32'] },
    interp_reset_result: { parameters: ['i32', 'i32'], results: ['i32'] },
    interp_set_return_options: { parameters: ['i32', 'i32'], results: ['i32'] },
    interp_get_return_options: { parameters: ['i32', 'i32'], results: ['i32'] },
    interp_get_script: { parameters: ['i32'], results: ['i32'] },
    interp_set_script: { parameters: ['i32', 'i32'], results: [] },

    bind_unknown: { parameters: ['i32', 'i32', 'i32', 'i32'], results: ['i32'] },

    trace_add: { parameters: ['i32', 'i32', 'i32', 'i32', 'i32'], results: ['i32'] },
    trace_remove: { parameters: ['i32', 'i32', 'i32', 'i32', 'i32'], results: ['i32'] },
    trace_info: { parameters: ['i32', 'i32', 'i32'], results: ['i32'] },

    foreign_is_foreign: { parameters: ['i32', 'i32'], results: ['i32'] },
    foreign_type_name: { parameters: ['i32', 'i32'], results: ['i32'] },
    foreign_string_rep: { parameters: ['i32', 'i32'], results: ['i32'] },
    foreign_methods: { parameters: ['i32', 'i32'], results: ['i32'] },
    foreign_invoke: { parameters: ['i32', 'i32', 'i32', 'i32'], results: ['i32'] },
    foreign_destroy: { parameters: ['i32', 'i32'], results: [] },
  };

  const fields = [
    // FeatherFrameOps
    'frame_push', 'frame_pop', 'frame_level', 'frame_set_active',
    'frame_size', 'frame_info', 'frame_set_namespace', 'frame_get_namespace',
    // FeatherVarOps
    'var_get', 'var_set', 'var_unset', 'var_exists', 'var_link', 'var_link_ns', 'var_names',
    // FeatherProcOps
    'proc_define', 'proc_exists', 'proc_params', 'proc_body', 'proc_names',
    'proc_resolve_namespace', 'proc_register_builtin', 'proc_lookup', 'proc_rename',
    // FeatherNamespaceOps
    'ns_create', 'ns_delete', 'ns_exists', 'ns_current', 'ns_parent', 'ns_children',
    'ns_get_var', 'ns_set_var', 'ns_var_exists', 'ns_unset_var',
    'ns_get_command', 'ns_set_command', 'ns_delete_command', 'ns_list_commands',
    'ns_get_exports', 'ns_set_exports', 'ns_is_exported', 'ns_copy_command',
    // FeatherStringOps
    'string_intern', 'string_get', 'string_concat', 'string_compare', 'string_regex_match',
    // FeatherRuneOps
    'rune_length', 'rune_at', 'rune_range', 'rune_to_upper', 'rune_to_lower', 'rune_fold',
    // FeatherListOps
    'list_is_nil', 'list_create', 'list_from', 'list_push', 'list_pop',
    'list_unshift', 'list_shift', 'list_length', 'list_at',
    'list_slice', 'list_set_at', 'list_splice', 'list_sort',
    // FeatherDictOps
    'dict_create', 'dict_is_dict', 'dict_from', 'dict_get', 'dict_set',
    'dict_exists', 'dict_remove', 'dict_size', 'dict_keys', 'dict_values',
    // FeatherIntOps
    'int_create', 'int_get',
    // FeatherDoubleOps
    'dbl_create', 'dbl_get',
    // FeatherInterpOps
    'interp_set_result', 'interp_get_result', 'interp_reset_result',
    'interp_set_return_options', 'interp_get_return_options',
    'interp_get_script', 'interp_set_script',
    // FeatherBindOps
    'bind_unknown',
    // FeatherTraceOps
    'trace_add', 'trace_remove', 'trace_info',
    // FeatherForeignOps
    'foreign_is_foreign', 'foreign_type_name', 'foreign_string_rep',
    'foreign_methods', 'foreign_invoke', 'foreign_destroy',
  ];

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

  wasmTable = new WebAssembly.Table({ initial: 128, element: 'anyfunc' });
  wasmMemory = new WebAssembly.Memory({ initial: 32 });

  wasmInstance = await WebAssembly.instantiate(wasmModule, {
    env: {
      memory: wasmMemory,
      __indirect_function_table: wasmTable,
    }
  });

  wasmMemory = wasmInstance.exports.memory || wasmMemory;
  wasmTable = wasmInstance.exports.__indirect_function_table || wasmTable;

  const buildHostOps = async () => {
    const STRUCT_SIZE = fields.length * 4;
    const opsPtr = wasmInstance.exports.alloc(STRUCT_SIZE);

    let offset = 0;
    for (const name of fields) {
      const fn = hostFunctions[name];
      const sig = signatures[name];
      if (!fn || !sig) {
        throw new Error(`Missing function or signature for: ${name}`);
      }
      const index = await addToTable(fn, sig);
      writeI32(opsPtr + offset, index);
      offset += 4;
    }

    return opsPtr;
  };

  const opsPtr = await buildHostOps();
  hostOpsPtr = opsPtr; // Make available to fireVarTraces/fireCmdTraces

  return {
    create() {
      const id = nextInterpId++;
      interpreters.set(id, new FeatherInterp(id));
      wasmInstance.exports.feather_interp_init(opsPtr, id);
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
      // Register the instance in foreignInstances so it can be looked up by handle name
      if (handleName) {
        interp.foreignInstances.set(handleName, { typeName, value, objHandle, handleName });
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

      const status = wasmInstance.exports.feather_parse_command(opsPtr, interpId, ctxPtr);

      wasmInstance.exports.free(ctxPtr);
      wasmInstance.exports.free(ptr);

      // Convert TCL_PARSE_DONE to TCL_PARSE_OK (empty script is OK)
      if (status === TCL_PARSE_DONE) {
        interp.result = interp.store({ type: 'list', items: [] });
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

      return { status, result: resultStr ? `{${resultStr}}` : '', errorMessage };
    },

    eval(interpId, script) {
      const [ptr, len] = writeString(script);
      const result = wasmInstance.exports.feather_script_eval(opsPtr, interpId, ptr, len, 0);
      wasmInstance.exports.free(ptr);

      const interp = interpreters.get(interpId);
      if (result === TCL_OK) {
        return interp.getString(interp.result);
      }
      // Handle TCL_RETURN at top level - apply the return options
      if (result === TCL_RETURN) {
        // Get return options and apply the code
        let code = TCL_OK;
        const opts = interp.returnOptions.get('current');
        if (opts) {
          const items = interp.getList(opts);
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
          return interp.getString(interp.result);
        }
        if (code === TCL_ERROR) {
          const error = new Error(interp.getString(interp.result));
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
        return interp.getString(interp.result);
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
      const error = new Error(interp.getString(interp.result));
      error.code = result;
      throw error;
    },

    getResult(interpId) {
      const interp = interpreters.get(interpId);
      return interp.getString(interp.result);
    },

    destroy(interpId) {
      interpreters.delete(interpId);
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
