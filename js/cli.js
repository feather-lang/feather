#!/usr/bin/env node --experimental-wasm-type-reflection
/**
 * feather CLI - A simple command-line interface for the feather TCL interpreter
 *
 * Usage:
 *   node cli.js                     # Start interactive REPL
 *   node cli.js script.tcl          # Execute a script file
 *   node cli.js -e 'puts hello'     # Execute inline script
 */

import { createFeather } from './feather.js';
import { readFileSync, existsSync } from 'fs';
import { createInterface } from 'readline';
import { dirname, join } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));

async function main() {
  const wasmPath = join(__dirname, 'feather.wasm');

  if (!existsSync(wasmPath)) {
    console.error(`Error: feather.wasm not found at ${wasmPath}`);
    console.error('Build feather with: zig build -Dtarget=wasm32-freestanding');
    process.exit(1);
  }

  const feather = await createFeather(wasmPath);
  const interp = feather.create();

  feather.register(interp, 'puts', (args) => {
    console.log(args.join(' '));
    return '';
  });

  feather.register(interp, 'gets', (args) => {
    throw new Error('gets not supported in CLI mode');
  });

  feather.register(interp, 'exit', (args) => {
    const code = args[0] ? parseInt(args[0], 10) : 0;
    process.exit(code);
  });

  feather.register(interp, 'source', (args) => {
    if (args.length !== 1) throw new Error('wrong # args: should be "source fileName"');
    const content = readFileSync(args[0], 'utf-8');
    return feather.eval(interp, content);
  });

  feather.register(interp, 'clock', (args) => {
    if (args[0] === 'seconds') {
      return Math.floor(Date.now() / 1000);
    }
    if (args[0] === 'milliseconds') {
      return Date.now();
    }
    throw new Error(`unknown clock subcommand "${args[0]}"`);
  });

  const argv = process.argv.slice(2);

  if (argv.length === 0) {
    await runRepl(feather, interp);
    return; // runRepl handles cleanup
  } else if (argv[0] === '-e') {
    if (argv.length < 2) {
      console.error('Error: -e requires a script argument');
      process.exit(1);
    }
    try {
      const result = feather.eval(interp, argv.slice(1).join(' '));
      if (result) console.log(result);
    } catch (e) {
      console.error(`Error: ${e.message}`);
      process.exit(1);
    }
  } else if (argv[0] === '-h' || argv[0] === '--help') {
    console.log(`
feather - A TCL interpreter

Usage:
  feather                     Start interactive REPL
  feather script.tcl          Execute a script file
  feather -e 'puts hello'     Execute inline script
  feather -h, --help          Show this help

Built-in commands:
  puts ?-nonewline? string    Print to stdout
  exit ?code?                 Exit with code (default 0)
  source fileName             Execute a script file
  clock seconds|milliseconds  Get current time
`);
  } else {
    const scriptPath = argv[0];
    if (!existsSync(scriptPath)) {
      console.error(`Error: file not found: ${scriptPath}`);
      process.exit(1);
    }
    try {
      const content = readFileSync(scriptPath, 'utf-8');
      feather.eval(interp, content);
    } catch (e) {
      console.error(`Error: ${e.message}`);
      process.exit(1);
    }
  }

  feather.destroy(interp);
}

function runRepl(feather, interp) {
  return new Promise((resolve) => {
    const rl = createInterface({
      input: process.stdin,
      output: process.stdout,
      prompt: 'feather> ',
    });

    console.log('feather TCL interpreter');
    console.log('Type "exit" to quit, "help" for commands');
    console.log('');

    rl.prompt();

    let buffer = '';
    let braceDepth = 0;

    rl.on('line', (line) => {
      buffer += (buffer ? '\n' : '') + line;

      for (const ch of line) {
        if (ch === '{') braceDepth++;
        else if (ch === '}') braceDepth--;
      }

      if (braceDepth > 0) {
        process.stdout.write('> ');
        return;
      }

      braceDepth = 0;

      if (buffer.trim() === 'help') {
        console.log(`
Available commands:
  puts string     - Print to stdout
  set var value   - Set a variable
  expr expression - Evaluate math expression
  proc name args body - Define a procedure
  if cond body    - Conditional execution
  foreach var list body - Loop over list
  while cond body - While loop
  exit ?code?     - Exit interpreter
  help            - Show this help
`);
        buffer = '';
        rl.prompt();
        return;
      }

      if (buffer.trim()) {
        try {
          const result = feather.eval(interp, buffer);
          if (result) console.log(result);
        } catch (e) {
          console.error(`Error: ${e.message}`);
        }
      }

      buffer = '';
      rl.prompt();
    });

    rl.on('close', () => {
      console.log('');
      feather.destroy(interp);
      resolve();
    });
  });
}

main().catch((e) => {
  console.error(`Fatal error: ${e.message}`);
  process.exit(1);
});
