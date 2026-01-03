#!/usr/bin/env -S node --experimental-wasm-type-reflection
/**
 * tester.js - JavaScript host for the feather test harness.
 * Mirrors cmd/feather-tester/main.go definitions.
 */

import { createFeather, TCL_PARSE_OK, TCL_PARSE_INCOMPLETE, TCL_PARSE_ERROR } from './feather.js';
import { createInterface } from 'readline';
import { readFileSync, writeSync, fstatSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

async function main() {
  // Check for benchmark mode
  if (process.argv.length > 2 && process.argv[2] === '--benchmark') {
    await runBenchmarkMode();
    return;
  }

  const wasmPath = join(__dirname, 'feather.wasm');
  const feather = await createFeather(wasmPath);
  const interp = feather.create();

  registerTestCommands(feather, interp);

  // Check if stdin is a TTY
  try {
    const stat = fstatSync(0);
    if (stat.isCharacterDevice()) {
      await runREPL(feather, interp);
      return;
    }
  } catch {
    // Not a TTY, proceed with script mode
  }

  await runScript(feather, interp);
}

function registerTestCommands(feather, interp) {
  // Set milestone variables
  feather.eval(interp, 'set milestone m1');
  feather.eval(interp, 'set current-step m1');

  // Test commands
  feather.register(interp, 'say-hello', () => {
    console.log('hello');
    return '';
  });

  feather.register(interp, 'echo', (args) => {
    console.log(args.join(' '));
    return '';
  });

  feather.register(interp, 'count', (args) => {
    return String(args.length);
  });

  feather.register(interp, 'list', (args) => {
    const parts = args.map(s => {
      if (s.length === 0) return '{}';
      if (/[\s{}]/.test(s)) return '{' + s + '}';
      return s;
    });
    return parts.join(' ');
  });

  // Register the Counter foreign type
  let nextCounterId = 1;
  const counters = new Map();

  feather.registerType(interp, 'Counter', {
    methods: {
      get: (c) => c.value,
      set: (c, val) => { c.value = parseInt(val, 10); return ''; },
      incr: (c) => ++c.value,
      add: (c, amount) => { c.value += parseInt(amount, 10); return c.value; },
    },
    destroy: (c) => { counters.delete(c.id); }
  });

  // Register Counter constructor command
  feather.register(interp, 'Counter', (args) => {
    if (args.length === 0) {
      throw new Error('wrong # args: should be "Counter subcommand ?arg ...?"');
    }
    if (args[0] !== 'new') {
      throw new Error(`unknown subcommand "${args[0]}": must be new`);
    }
    const id = nextCounterId++;
    const counter = { id, value: 0 };
    counters.set(id, counter);

    const handle = `counter${id}`;
    const foreignHandle = feather.createForeign(interp, 'Counter', counter, handle);

    // Define method signatures: [expectedArgCount, needsIntArg]
    const methodDefs = {
      get: { argc: 0, fn: () => counter.value },
      set: { argc: 1, intArg: 1, fn: (val) => { counter.value = val; return ''; } },
      incr: { argc: 0, fn: () => ++counter.value },
      add: { argc: 1, intArg: 1, fn: (amount) => { counter.value += amount; return counter.value; } },
      destroy: { argc: 0, fn: () => {
        counters.delete(id);
        feather.destroyForeign(interp, handle);
        feather.register(interp, handle, () => { throw new Error(`invalid command name "${handle}"`); });
        return '';
      }}
    };

    // Register the object-as-command
    feather.register(interp, handle, (methodArgs) => {
      if (methodArgs.length === 0) {
        throw new Error(`wrong # args: should be "${handle} method ?arg ...?"`);
      }
      const method = methodArgs[0];
      const rest = methodArgs.slice(1);
      const def = methodDefs[method];
      if (!def) {
        const methodList = Object.keys(methodDefs).join(', ');
        throw new Error(`unknown method "${method}": must be ${methodList}`);
      }
      // Check argument count
      if (rest.length !== def.argc) {
        throw new Error(`wrong # args: expected ${def.argc}, got ${rest.length}`);
      }
      // Convert integer arguments if needed
      const convertedArgs = rest.map((arg, idx) => {
        if (def.intArg === idx + 1) {
          const num = parseInt(arg, 10);
          if (isNaN(num)) {
            throw new Error(`argument ${idx + 1}: expected integer but got "${arg}"`);
          }
          return num;
        }
        return arg;
      });
      return String(def.fn(...convertedArgs) ?? '');
    });

    return handle;
  });
}

async function runREPL(feather, interp) {
  const rl = createInterface({
    input: process.stdin,
    output: process.stdout,
  });

  let inputBuffer = '';

  const prompt = () => {
    rl.question(inputBuffer === '' ? '% ' : '> ', (line) => {
      if (line === undefined) {
        rl.close();
        return;
      }

      inputBuffer = inputBuffer ? inputBuffer + '\n' + line : line;

      // Simple incomplete check: count braces
      const opens = (inputBuffer.match(/{/g) || []).length;
      const closes = (inputBuffer.match(/}/g) || []).length;
      if (opens > closes) {
        prompt();
        return;
      }

      try {
        const result = feather.eval(interp, inputBuffer);
        if (result !== '') console.log(result);
      } catch (e) {
        console.error(`error: ${e.message}`);
      }
      inputBuffer = '';
      prompt();
    });
  };

  return new Promise((resolve) => {
    rl.on('close', () => {
      console.log('');
      feather.destroy(interp);
      resolve();
    });
    prompt();
  });
}

async function runScript(feather, interp) {
  const script = readFileSync(0, 'utf-8');

  // Check parse status first (like Go version)
  const parseResult = feather.parse(interp, script);
  if (parseResult.status === TCL_PARSE_INCOMPLETE) {
    writeHarnessResult('TCL_OK', parseResult.result, '');
    process.exit(2);
  }
  if (parseResult.status === TCL_PARSE_ERROR) {
    writeHarnessResult('TCL_ERROR', parseResult.result, parseResult.errorMessage);
    process.exit(3);
  }

  try {
    const result = feather.eval(interp, script);
    if (result !== '') console.log(result);
    writeHarnessResult('TCL_OK', result, '');
  } catch (e) {
    console.log(e.message);
    writeHarnessResult('TCL_ERROR', '', e.message);
    process.exit(1);
  }
}

function writeHarnessResult(returnCode, result, errorMsg) {
  if (process.env.FEATHER_IN_HARNESS !== '1') return;

  // Write to fd 3 (harness communication channel)
  let output = `return: ${returnCode}\n`;
  if (result !== '') output += `result: ${result}\n`;
  if (errorMsg !== '') output += `error: ${errorMsg}\n`;

  try {
    writeSync(3, output);
  } catch {
    // fd 3 not available, ignore
  }
}

async function runBenchmarkMode() {
  const wasmPath = join(__dirname, 'feather.wasm');
  const feather = await createFeather(wasmPath);
  const interp = feather.create();

  registerTestCommands(feather, interp);

  // Read benchmarks from stdin
  const input = readFileSync(0, 'utf-8');
  let benchmarks;
  try {
    benchmarks = JSON.parse(input);
  } catch (e) {
    console.error(`error reading benchmarks: ${e.message}`);
    process.exit(1);
  }

  // Run each benchmark
  for (const b of benchmarks) {
    const result = runSingleBenchmark(feather, interp, b);

    // Write result as JSON to fd 3
    const resultJSON = JSON.stringify(result);
    try {
      writeSync(3, resultJSON + '\n');
    } catch {
      console.error('error: harness channel not available');
      process.exit(1);
    }
  }
}

function runSingleBenchmark(feather, interp, b) {
  const result = {
    Benchmark: b,
    Success: true,
    TotalTime: 0,
    AvgTime: 0,
    MinTime: 0,
    MaxTime: 0,
    Iterations: 0,
    OpsPerSecond: 0,
    Error: ''
  };

  // Run setup if provided
  if (b.Setup) {
    try {
      feather.eval(interp, b.Setup);
    } catch (e) {
      result.Success = false;
      result.Error = `setup failed: ${e.message}`;
      return result;
    }
  }

  // Warmup iterations
  for (let w = 0; w < (b.Warmup || 0); w++) {
    try {
      feather.eval(interp, b.Script);
    } catch (e) {
      result.Success = false;
      result.Error = `warmup failed: ${e.message}`;
      return result;
    }
  }

  // Measured iterations
  const iterations = b.Iterations || 1000;
  let totalTime = 0;
  let minTime = Infinity;
  let maxTime = 0;

  for (let iter = 0; iter < iterations; iter++) {
    const start = process.hrtime.bigint();
    try {
      feather.eval(interp, b.Script);
    } catch (e) {
      result.Success = false;
      result.Error = `iteration ${iter} failed: ${e.message}`;
      return result;
    }
    const end = process.hrtime.bigint();
    const elapsed = Number(end - start); // nanoseconds

    totalTime += elapsed;
    if (elapsed < minTime) minTime = elapsed;
    if (elapsed > maxTime) maxTime = elapsed;
    result.Iterations++;
  }

  // Calculate statistics (convert from nanoseconds to time.Duration format)
  result.TotalTime = totalTime;
  result.AvgTime = Math.floor(totalTime / iterations);
  result.MinTime = minTime;
  result.MaxTime = maxTime;
  if (result.AvgTime > 0) {
    result.OpsPerSecond = 1e9 / result.AvgTime; // 1 billion ns per second
  }

  return result;
}

main().catch((e) => {
  console.error(`Fatal error: ${e.message}`);
  process.exit(1);
});
