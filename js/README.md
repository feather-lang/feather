# feather JavaScript Host

JavaScript/TypeScript host for the feather TCL interpreter, compiled to WebAssembly.

Works in both **Node.js** and **browsers** using ES modules.

## Quick Start

### Build

Build feather.wasm using Zig (via mise):

```bash
cd js
mise run build
```

### Node.js

```javascript
import { createFeather } from './feather.js';

const feather = await createFeather('./feather.wasm');
const interp = feather.create();

// Register host commands
feather.register(interp, 'puts', (args) => {
  console.log(args.join(' '));
});

// Evaluate TCL scripts
feather.eval(interp, 'set x 10');
feather.eval(interp, 'puts "x = $x"');

// Clean up
feather.destroy(interp);
```

### Browser

```html
<script type="module">
  import { createFeather } from './feather.js';

  const feather = await createFeather('./feather.wasm');
  const interp = feather.create();

  feather.register(interp, 'alert', (args) => {
    window.alert(args.join(' '));
  });

  feather.eval(interp, 'alert "Hello from TCL!"');
</script>
```

## CLI

Run scripts from the command line:

```bash
# Start REPL
node cli.js

# Execute a script file
node cli.js script.tcl

# Execute inline script
node cli.js -e 'puts [expr {2 + 2}]'
```

## API

### `createFeather(wasmSource)`

Create a feather host instance.

- `wasmSource`: Path to `.wasm` file (Node.js), URL string, `ArrayBuffer`, or `Response`
- Returns: `Promise<Feather>`

### `feather.create()`

Create a new interpreter instance.

- Returns: `number` (interpreter ID)

### `feather.register(interpId, name, fn)`

Register a host command callable from TCL.

- `interpId`: Interpreter ID
- `name`: Command name
- `fn`: `(args: string[]) => string | number | void`

### `feather.eval(interpId, script)`

Evaluate a TCL script.

- `interpId`: Interpreter ID
- `script`: TCL source code
- Returns: Result string
- Throws: `TclError` on failure

### `feather.destroy(interpId)`

Destroy an interpreter instance.

## Foreign Objects

Expose JavaScript objects to TCL:

```javascript
// Define a type
feather.registerType(interp, 'Counter', {
  methods: {
    incr: (counter) => ++counter.value,
    get: (counter) => counter.value,
    set: (counter, val) => counter.value = parseInt(val),
  },
  destroy: (counter) => console.log('Counter destroyed'),
});

// Create instances
const handle = feather.createForeign(interp, 'Counter', { value: 0 }, 'counter1');

// Use from TCL (requires host command to create)
feather.register(interp, 'Counter', (args) => {
  if (args[0] === 'new') {
    return feather.createForeign(interp, 'Counter', { value: 0 });
  }
});
```

## Browser Demo

Open `index.html` in a browser to try the interactive demo. Make sure `feather.wasm` is in the same directory.

## TypeScript

Type definitions are included in `feather.d.ts`.

```typescript
import { createFeather, Feather, TCL_OK } from './feather.js';

const feather: Feather = await createFeather('./feather.wasm');
```

## Architecture

The host provides implementations for all `feather_host_*` functions as direct WASM imports:

1. JavaScript implements all 97 host callbacks (frame, var, proc, string, list, etc.)
2. These functions are provided in the `env` imports object during WASM instantiation
3. The C code calls these via `extern` declarations that resolve to WASM imports
4. All API calls pass `0` (NULL) for the `ops` parameter

Objects live in JavaScript (managed by the host), while the C code only handles opaque integer handles.

See [WASM.md](../WASM.md) for detailed architecture documentation.

## License

MIT
