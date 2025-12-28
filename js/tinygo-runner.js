#!/usr/bin/env node
/**
 * tinygo-runner.js - Node.js host for the TinyGo-compiled feather-tester WASM.
 *
 * This bridges the Go WASM module to the feather.js C interpreter.
 */

import { createFeather } from './feather.js';
import { readFileSync, writeSync, fstatSync, readSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';
import { WASI } from 'wasi';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

async function main() {
  // Load the C interpreter WASM
  const cWasmPath = join(__dirname, 'feather.wasm');
  const feather = await createFeather(cWasmPath);

  // Load the TinyGo WASM
  const goWasmPath = join(__dirname, '..', 'bin', 'feather-tester.wasm');
  const goWasmBytes = readFileSync(goWasmPath);

  // State for Go WASM
  let goMemory;
  let goInstance;
  let currentInterp = null;
  const commandCallbacks = new Map();

  // Buffer for reading stdin
  let stdinBuffer = null;
  let stdinPos = 0;

  // Pre-read stdin if not a TTY
  try {
    const stat = fstatSync(0);
    if (!stat.isCharacterDevice()) {
      // Read all of stdin
      const chunks = [];
      const buf = Buffer.alloc(4096);
      let bytesRead;
      while ((bytesRead = readSync(0, buf)) > 0) {
        chunks.push(buf.slice(0, bytesRead));
      }
      stdinBuffer = Buffer.concat(chunks);
    }
  } catch {
    // stdin not available
  }

  // Helper to read string from Go WASM memory
  const readGoString = (ptr, len) => {
    if (!ptr || !len) return '';
    const bytes = new Uint8Array(goMemory.buffer, ptr, len);
    return new TextDecoder().decode(bytes);
  };

  // Helper to write string to Go WASM memory
  const writeGoString = (str, ptr, lenPtr) => {
    if (!ptr || !lenPtr) return;
    const bytes = new TextEncoder().encode(str);
    const view = new DataView(goMemory.buffer);
    view.setUint32(lenPtr, bytes.length, true);
    if (bytes.length > 0) {
      new Uint8Array(goMemory.buffer, ptr, bytes.length).set(bytes);
    }
  };

  // Feather module imports for Go WASM
  const featherImports = {
    create_interp: () => {
      currentInterp = feather.create();
      return currentInterp;
    },

    destroy_interp: (id) => {
      feather.destroy(id);
      currentInterp = null;
    },

    eval: (interpId, scriptPtr, scriptLen, resultPtr, resultLenPtr) => {
      const script = readGoString(scriptPtr, scriptLen);
      try {
        const result = feather.eval(interpId, script);
        writeGoString(result, resultPtr, resultLenPtr);
        return 0; // TCL_OK
      } catch (e) {
        writeGoString(e.message, resultPtr, resultLenPtr);
        return 1; // TCL_ERROR
      }
    },

    parse: (interpId, scriptPtr, scriptLen) => {
      const script = readGoString(scriptPtr, scriptLen);
      const parseResult = feather.parse(interpId, script);
      return parseResult.status;
    },

    register_command: (interpId, namePtr, nameLen, callbackId) => {
      const name = readGoString(namePtr, nameLen);
      commandCallbacks.set(callbackId, name);

      feather.register(interpId, name, (args) => {
        // Call back to Go WASM
        const argsStr = args.join(' ');
        const argsBytes = new TextEncoder().encode(argsStr);

        // Allocate memory in Go for args and result
        const argsPtr = goInstance.exports.malloc(argsBytes.length || 1);
        if (argsBytes.length > 0) {
          new Uint8Array(goMemory.buffer, argsPtr, argsBytes.length).set(argsBytes);
        }

        const resultBufSize = 4096;
        const resultPtr = goInstance.exports.malloc(resultBufSize);
        const resultLenPtr = goInstance.exports.malloc(4);

        // Call the Go callback
        const status = goInstance.exports.feather_command_callback(
          callbackId, argsPtr, argsBytes.length, resultPtr, resultLenPtr
        );

        // Read result
        const view = new DataView(goMemory.buffer);
        const resultLen = view.getUint32(resultLenPtr, true);
        const result = readGoString(resultPtr, resultLen);

        // Free memory
        goInstance.exports.free(argsPtr);
        goInstance.exports.free(resultPtr);
        goInstance.exports.free(resultLenPtr);

        if (status !== 0) {
          throw new Error(result);
        }
        return result;
      });
    },

    set_var: (interpId, namePtr, nameLen, valuePtr, valueLen) => {
      const name = readGoString(namePtr, nameLen);
      const value = readGoString(valuePtr, valueLen);
      feather.eval(interpId, `set ${name} {${value}}`);
    },

    write_stdout: (ptr, len) => {
      const str = readGoString(ptr, len);
      process.stdout.write(str);
    },

    write_stderr: (ptr, len) => {
      const str = readGoString(ptr, len);
      process.stderr.write(str);
    },

    write_fd: (fd, ptr, len) => {
      const str = readGoString(ptr, len);
      try {
        writeSync(fd, str);
      } catch {
        // fd not available, ignore
      }
    },

    read_stdin: (bufPtr, bufLen) => {
      // Read from pre-buffered stdin
      if (stdinBuffer !== null) {
        const remaining = stdinBuffer.length - stdinPos;
        if (remaining <= 0) return 0;
        const toRead = Math.min(remaining, bufLen);
        const chunk = stdinBuffer.slice(stdinPos, stdinPos + toRead);
        new Uint8Array(goMemory.buffer, bufPtr, toRead).set(chunk);
        stdinPos += toRead;
        return toRead;
      }
      // For TTY, try synchronous read (may not work in all environments)
      try {
        const buf = Buffer.alloc(bufLen);
        const bytesRead = readSync(0, buf, 0, bufLen, null);
        if (bytesRead > 0) {
          new Uint8Array(goMemory.buffer, bufPtr, bytesRead).set(buf.slice(0, bytesRead));
        }
        return bytesRead;
      } catch {
        return 0;
      }
    },

    is_tty: () => {
      // If we pre-buffered stdin, it's not a TTY
      if (stdinBuffer !== null) return 0;
      try {
        const stat = fstatSync(0);
        return stat.isCharacterDevice() ? 1 : 0;
      } catch {
        return 0;
      }
    },

    get_env: (namePtr, nameLen, valuePtr, valueLenPtr) => {
      const name = readGoString(namePtr, nameLen);
      const value = process.env[name] || '';
      if (!value) return 0;
      writeGoString(value, valuePtr, valueLenPtr);
      return 1;
    },
  };

  // Create WASI instance for TinyGo
  const wasi = new WASI({
    version: 'preview1',
    args: process.argv.slice(1),
    env: process.env,
    preopens: {},
  });

  // Compile and instantiate Go WASM
  const goModule = await WebAssembly.compile(goWasmBytes);

  // Get WASI imports
  const wasiImports = wasi.getImportObject();

  // Merge imports
  const imports = {
    ...wasiImports,
    feather: featherImports,
  };

  goInstance = await WebAssembly.instantiate(goModule, imports);
  goMemory = goInstance.exports.memory;

  // Start the WASI application
  try {
    wasi.start(goInstance);
  } catch (e) {
    if (e.code !== undefined) {
      process.exit(e.code);
    }
    throw e;
  }
}

main().catch((e) => {
  console.error(`Fatal error: ${e.message}`);
  console.error(e.stack);
  process.exit(1);
});
