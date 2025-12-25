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

const TCL_CMD_NONE = 0;
const TCL_CMD_BUILTIN = 1;
const TCL_CMD_PROC = 2;

class FeatherInterp {
  constructor(id) {
    this.id = id;
    this.objects = new Map();
    this.nextHandle = 1;
    this.result = 0;
    this.frames = [{ vars: new Map(), cmd: 0, args: 0, ns: '::' }];
    this.activeLevel = 0;
    this.procs = new Map();
    this.builtins = new Map();
    this.hostCommands = new Map();
    this.namespaces = new Map([['', { vars: new Map(), children: new Map(), exports: [] }]]);
    this.traces = { variable: new Map(), command: new Map() };
    this.returnOptions = new Map();
    this.scriptPath = '';
    this.foreignTypes = new Map();
  }

  store(obj) {
    const handle = this.nextHandle++;
    this.objects.set(handle, obj);
    return handle;
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
    if (obj.type === 'list') return obj.items.map(h => this.getString(h)).join(' ');
    if (obj.type === 'dict') {
      const parts = [];
      for (const [k, v] of obj.entries) {
        parts.push(this.getString(k), this.getString(v));
      }
      return parts.join(' ');
    }
    if (obj.type === 'foreign') return obj.stringRep || `<${obj.typeName}:${handle}>`;
    return String(obj);
  }

  getList(handle) {
    if (handle === 0) return [];
    const obj = this.get(handle);
    if (!obj) return [];
    if (obj.type === 'list') return obj.items;
    const str = this.getString(handle);
    return this.parseList(str);
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
        const ns = { vars: new Map(), children: new Map(), exports: [] };
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

  const addToTable = (fn, signature) => {
    const wasmFn = new WebAssembly.Function(signature, fn);
    const index = wasmTable.length;
    wasmTable.grow(1);
    wasmTable.set(index, wasmFn);
    return index;
  };

  const hostFunctions = {
    // Frame operations
    frame_push: (interpId, cmd, args) => {
      const interp = interpreters.get(interpId);
      const parentNs = interp.frames[interp.frames.length - 1].ns;
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
      if (entry.link) {
        const targetFrame = interp.frames[entry.link.level];
        return targetFrame?.vars.get(entry.link.name)?.value || 0;
      }
      if (entry.nsLink) {
        const ns = interp.getNamespace(entry.nsLink.ns);
        return ns?.vars.get(entry.nsLink.name) || 0;
      }
      return entry.value || 0;
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
    },
    var_unset: (interpId, name) => {
      const interp = interpreters.get(interpId);
      const varName = interp.getString(name);
      interp.currentFrame().vars.delete(varName);
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
      const list = { type: 'list', items: names.map(n => interp.store({ type: 'string', value: n })) };
      return interp.store(list);
    },

    // Proc operations
    proc_define: (interpId, name, params, body) => {
      const interp = interpreters.get(interpId);
      const procName = interp.getString(name);
      interp.procs.set(procName, { params, body });
    },
    proc_exists: (interpId, name) => {
      const interp = interpreters.get(interpId);
      const procName = interp.getString(name);
      return interp.procs.has(procName) ? 1 : 0;
    },
    proc_params: (interpId, name, resultPtr) => {
      const interp = interpreters.get(interpId);
      const procName = interp.getString(name);
      const proc = interp.procs.get(procName);
      if (proc) {
        writeI32(resultPtr, proc.params);
        return TCL_OK;
      }
      return TCL_ERROR;
    },
    proc_body: (interpId, name, resultPtr) => {
      const interp = interpreters.get(interpId);
      const procName = interp.getString(name);
      const proc = interp.procs.get(procName);
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
      if (interp.procs.has(oldN)) {
        const proc = interp.procs.get(oldN);
        interp.procs.delete(oldN);
        if (newN) interp.procs.set(newN, proc);
        return TCL_OK;
      }
      if (interp.builtins.has(oldN)) {
        const fn = interp.builtins.get(oldN);
        interp.builtins.delete(oldN);
        if (newN) interp.builtins.set(newN, fn);
        return TCL_OK;
      }
      return TCL_ERROR;
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
      if (nsPath === '') return TCL_ERROR;
      interp.namespaces.delete(nsPath);
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
      const children = [...namespace.children.values()].map(c =>
        interp.store({ type: 'string', value: '::' + c })
      );
      return interp.store({ type: 'list', items: children });
    },
    ns_get_var: (interpId, ns, name) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns);
      const varName = interp.getString(name);
      const namespace = interp.getNamespace(nsPath);
      return namespace?.vars.get(varName) || 0;
    },
    ns_set_var: (interpId, ns, name, value) => {
      const interp = interpreters.get(interpId);
      const nsPath = interp.getString(ns);
      const varName = interp.getString(name);
      const namespace = interp.ensureNamespace(nsPath);
      namespace.vars.set(varName, value);
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
    ns_set_command: (interpId, ns, name, kind, fn, params, body) => {
      const interp = interpreters.get(interpId);
      const procName = interp.getString(name);
      if (kind === TCL_CMD_BUILTIN) {
        interp.builtins.set(procName, fn);
      } else if (kind === TCL_CMD_PROC) {
        interp.procs.set(procName, { params, body });
      }
    },
    ns_delete_command: (interpId, ns, name) => {
      const interp = interpreters.get(interpId);
      const procName = interp.getString(name);
      if (interp.procs.delete(procName) || interp.builtins.delete(procName)) {
        return TCL_OK;
      }
      return TCL_ERROR;
    },
    ns_list_commands: (interpId, ns) => {
      const interp = interpreters.get(interpId);
      const names = [...interp.procs.keys(), ...interp.builtins.keys()];
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
      const src = interp.getString(srcName);
      const dst = interp.getString(dstName);
      if (interp.builtins.has(src)) {
        interp.builtins.set(dst, interp.builtins.get(src));
        return TCL_OK;
      }
      if (interp.procs.has(src)) {
        interp.procs.set(dst, interp.procs.get(src));
        return TCL_OK;
      }
      return TCL_ERROR;
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
      const result = interp.getString(a) + interp.getString(b);
      return interp.store({ type: 'string', value: result });
    },
    string_compare: (interpId, a, b) => {
      const interp = interpreters.get(interpId);
      const strA = interp.getString(a);
      const strB = interp.getString(b);
      return strA < strB ? -1 : strA > strB ? 1 : 0;
    },
    string_regex_match: (interpId, pattern, string, resultPtr) => {
      const interp = interpreters.get(interpId);
      try {
        const regex = new RegExp(interp.getString(pattern));
        const matched = regex.test(interp.getString(string));
        writeI32(resultPtr, matched ? 1 : 0);
        return TCL_OK;
      } catch (e) {
        interp.result = interp.store({ type: 'string', value: `bad regex: ${e.message}` });
        return TCL_ERROR;
      }
    },

    // Rune operations
    rune_length: (interpId, str) => {
      const interp = interpreters.get(interpId);
      return [...interp.getString(str)].length;
    },
    rune_at: (interpId, str, index) => {
      const interp = interpreters.get(interpId);
      const chars = [...interp.getString(str)];
      if (index < 0 || index >= chars.length) return interp.store({ type: 'string', value: '' });
      return interp.store({ type: 'string', value: chars[index] });
    },
    rune_range: (interpId, str, first, last) => {
      const interp = interpreters.get(interpId);
      const chars = [...interp.getString(str)];
      const start = Math.max(0, Number(first));
      const end = Math.min(chars.length - 1, Number(last));
      if (start > end) return interp.store({ type: 'string', value: '' });
      return interp.store({ type: 'string', value: chars.slice(start, end + 1).join('') });
    },
    rune_to_upper: (interpId, str) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'string', value: interp.getString(str).toUpperCase() });
    },
    rune_to_lower: (interpId, str) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'string', value: interp.getString(str).toLowerCase() });
    },
    rune_fold: (interpId, str) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'string', value: interp.getString(str).toLowerCase() });
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
      }
      return list;
    },
    list_pop: (interpId, list) => {
      const interp = interpreters.get(interpId);
      const listObj = interp.get(list);
      if (listObj?.type === 'list' && listObj.items.length > 0) {
        return listObj.items.pop();
      }
      return 0;
    },
    list_unshift: (interpId, list, item) => {
      const interp = interpreters.get(interpId);
      const listObj = interp.get(list);
      if (listObj?.type === 'list') {
        listObj.items.unshift(item);
      }
      return list;
    },
    list_shift: (interpId, list) => {
      const interp = interpreters.get(interpId);
      const listObj = interp.get(list);
      if (listObj?.type === 'list' && listObj.items.length > 0) {
        return listObj.items.shift();
      }
      return 0;
    },
    list_length: (interpId, list) => {
      const interp = interpreters.get(interpId);
      const listObj = interp.get(list);
      if (listObj?.type === 'list') {
        return listObj.items.length;
      }
      return 0;
    },
    list_at: (interpId, list, index) => {
      const interp = interpreters.get(interpId);
      const listObj = interp.get(list);
      if (listObj?.type === 'list' && index < listObj.items.length) {
        return listObj.items[index];
      }
      return 0;
    },
    list_slice: (interpId, list, first, last) => {
      const interp = interpreters.get(interpId);
      const listObj = interp.get(list);
      if (listObj?.type === 'list') {
        const sliced = listObj.items.slice(Number(first), Number(last) + 1);
        return interp.store({ type: 'list', items: sliced });
      }
      return interp.store({ type: 'list', items: [] });
    },
    list_set_at: (interpId, list, index, value) => {
      const interp = interpreters.get(interpId);
      const listObj = interp.get(list);
      if (listObj?.type === 'list' && index < listObj.items.length) {
        listObj.items[index] = value;
        return TCL_OK;
      }
      return TCL_ERROR;
    },
    list_splice: (interpId, list, first, deleteCount, insertions) => {
      const interp = interpreters.get(interpId);
      const listObj = interp.get(list);
      if (listObj?.type === 'list') {
        const toInsert = interp.getList(insertions);
        listObj.items.splice(Number(first), Number(deleteCount), ...toInsert);
      }
      return list;
    },
    list_sort: (interpId, list, cmpFn, ctx) => {
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
      const dictObj = interp.get(dict);
      if (dictObj?.type !== 'dict') return 0;
      const keyStr = interp.getString(key);
      for (const [k, v] of dictObj.entries) {
        if (interp.getString(k) === keyStr) return v;
      }
      return 0;
    },
    dict_set: (interpId, dict, key, value) => {
      const interp = interpreters.get(interpId);
      let dictObj = interp.get(dict);
      if (!dictObj || dictObj.type !== 'dict') {
        dictObj = { type: 'dict', entries: [] };
        dict = interp.store(dictObj);
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
      const dictObj = interp.get(dict);
      if (dictObj?.type !== 'dict') return 0;
      const keyStr = interp.getString(key);
      return dictObj.entries.some(([k]) => interp.getString(k) === keyStr) ? 1 : 0;
    },
    dict_remove: (interpId, dict, key) => {
      const interp = interpreters.get(interpId);
      const dictObj = interp.get(dict);
      if (dictObj?.type !== 'dict') return dict;
      const keyStr = interp.getString(key);
      dictObj.entries = dictObj.entries.filter(([k]) => interp.getString(k) !== keyStr);
      return dict;
    },
    dict_size: (interpId, dict) => {
      const interp = interpreters.get(interpId);
      const dictObj = interp.get(dict);
      return dictObj?.type === 'dict' ? dictObj.entries.length : 0;
    },
    dict_keys: (interpId, dict) => {
      const interp = interpreters.get(interpId);
      const dictObj = interp.get(dict);
      if (dictObj?.type !== 'dict') return interp.store({ type: 'list', items: [] });
      return interp.store({ type: 'list', items: dictObj.entries.map(([k]) => k) });
    },
    dict_values: (interpId, dict) => {
      const interp = interpreters.get(interpId);
      const dictObj = interp.get(dict);
      if (dictObj?.type !== 'dict') return interp.store({ type: 'list', items: [] });
      return interp.store({ type: 'list', items: dictObj.entries.map(([, v]) => v) });
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
      const str = interp.getString(handle);
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
      const str = interp.getString(handle);
      const num = parseFloat(str);
      if (!isNaN(num)) {
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
      return TCL_OK;
    },
    interp_get_return_options: (interpId, code) => {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'dict', entries: [] });
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
      if (!hostFn) return TCL_ERROR;

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
        const pair = interp.store({
          type: 'list',
          items: [
            interp.store({ type: 'string', value: t.ops }),
            t.script
          ]
        });
        return pair;
      });
      return interp.store({ type: 'list', items });
    },

    // Foreign object operations
    foreign_is_foreign: (interpId, obj) => {
      const interp = interpreters.get(interpId);
      const o = interp.get(obj);
      return o?.type === 'foreign' ? 1 : 0;
    },
    foreign_type_name: (interpId, obj) => {
      const interp = interpreters.get(interpId);
      const o = interp.get(obj);
      if (o?.type !== 'foreign') return 0;
      return interp.store({ type: 'string', value: o.typeName });
    },
    foreign_string_rep: (interpId, obj) => {
      const interp = interpreters.get(interpId);
      const o = interp.get(obj);
      if (o?.type !== 'foreign') return 0;
      return interp.store({ type: 'string', value: o.stringRep || `<${o.typeName}:${obj}>` });
    },
    foreign_methods: (interpId, obj) => {
      const interp = interpreters.get(interpId);
      const o = interp.get(obj);
      if (o?.type !== 'foreign') return interp.store({ type: 'list', items: [] });
      const typeDef = interp.foreignTypes.get(o.typeName);
      if (!typeDef) return interp.store({ type: 'list', items: [] });
      const methods = Object.keys(typeDef.methods || {});
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

  const buildHostOps = () => {
    const STRUCT_SIZE = fields.length * 4;
    const opsPtr = wasmInstance.exports.alloc(STRUCT_SIZE);

    let offset = 0;
    for (const name of fields) {
      const fn = hostFunctions[name];
      const sig = signatures[name];
      if (!fn || !sig) {
        throw new Error(`Missing function or signature for: ${name}`);
      }
      const index = addToTable(fn, sig);
      writeI32(opsPtr + offset, index);
      offset += 4;
    }

    return opsPtr;
  };

  const opsPtr = buildHostOps();

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

    createForeign(interpId, typeName, value, stringRep) {
      const interp = interpreters.get(interpId);
      return interp.store({ type: 'foreign', typeName, value, stringRep });
    },

    eval(interpId, script) {
      const [ptr, len] = writeString(script);
      const result = wasmInstance.exports.feather_script_eval(opsPtr, interpId, ptr, len, 0);
      wasmInstance.exports.free(ptr);

      const interp = interpreters.get(interpId);
      if (result !== TCL_OK) {
        const error = new Error(interp.getString(interp.result));
        error.code = result;
        throw error;
      }
      return interp.getString(interp.result);
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

export { createFeather, TCL_OK, TCL_ERROR, TCL_RETURN, TCL_BREAK, TCL_CONTINUE };
