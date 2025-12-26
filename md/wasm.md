<script setup>
import WasmPlayground from '../.vitepress/components/WasmPlayground.vue'

const tryItJs = `// JavaScript state that Feather commands can modify
const state = { count: 0, items: [] };

register('bump', () => ++state.count);
register('get_count', () => state.count);
register('push', (args) => {
  state.items.push(args[0]);
  return state.items.length;
});
register('items', () => state.items.join(', '));`

const tryItTcl = `puts "Count: [get_count]"
bump
bump
bump
puts "After 3 bumps: [get_count]"

push "apple"
push "banana"
push "cherry"
puts "Items: [items]"`

const errorJs = `register('divide', (args) => {
  const divisor = Number(args[1]);
  if (divisor === 0) {
    throw new Error('division by zero');
  }
  return Number(args[0]) / divisor;
});`

const errorTcl = `puts [divide 10 2]
puts [divide 10 0]`

const typeJs = `// Register a foreign type with methods
feather.registerType(interp, 'Counter', {
  methods: {
    get: (c) => c.value,
    set: (c, val) => c.value = Number(val),
    add: (c, n) => c.value += Number(n || 1),
  },
});

// Register constructor command "Counter new"
let nextId = 0;
register('Counter', (args) => {
  if (args[0] !== 'new') throw new Error('unknown subcommand');
  const name = 'counter' + (++nextId);
  feather.createForeign(interp, 'Counter', { value: 0 }, name);
  return name;
});`

const typeTcl = `set c [Counter new]
puts "Type: [info type $c]"
puts "Methods: [info methods $c]"
puts "Initial: [$c get]"
$c add
$c add 5
puts "After adds: [$c get]"
$c set 100
puts "After set: [$c get]"`
</script>

# WASM

::: danger Early Alpha - Expect Breakage
The WASM build is alpha quality. The API isn't stable yet and there
are known bugs; despite the rough edges it is usable and useful
already.

Take it for a spin!
:::

Feather compiles to WebAssembly, allowing you to embed a TCL interpreter in Node.js applications and web browsers.

## Installation

Download these files:

- <a href="/feather.js" download>feather.js</a> - ES module with JavaScript bindings
- <a href="/feather.wasm" download>feather.wasm</a> - WebAssembly binary

## Node.js

```javascript
import { createFeather } from './feather.js';

const feather = await createFeather('./feather.wasm');
const interp = feather.create();

// Register host commands
feather.register(interp, 'puts', (args) => {
  console.log(args.join(' '));
});

// Evaluate TCL code
const result = feather.eval(interp, 'expr {2 + 2}');
console.log(result); // "4"

// Clean up
feather.destroy(interp);
```

## Browser

```html
<script type="module">
import { createFeather } from './feather.js';

const feather = await createFeather('/feather.wasm');
const interp = feather.create();

feather.register(interp, 'puts', (args) => {
  document.body.innerText += args.join(' ') + '\n';
});

feather.eval(interp, 'puts "Hello from Feather!"');
</script>
```

## How Not to Suffer

Remember, Feather is a thin _glue_ layer to expose existing functionality of your application.

Do:

- keep all state you need to manage in JavaScript,
- keep your custom commands short: they should parse arguments and call your application code

Don't:

- manage state in Feather,
- write complicated custom commands: these are thin entrypoints only

## Try It

<WasmPlayground :js="tryItJs" :tcl="tryItTcl" />

## API Reference

### `createFeather(wasmSource)`

Creates a Feather runtime. Returns a promise.

- `wasmSource` - Path to WASM file, URL, `ArrayBuffer`, or `Response`

### `feather.create()`

Creates a new interpreter. Returns an interpreter ID.

### `feather.eval(interpId, script)`

Evaluates TCL code. Returns the result as a string. Throws on error.

### `feather.register(interpId, name, fn)`

Registers a host command. `fn` receives an array of strings.

To signal an error, throw an exception:

<WasmPlayground :js="errorJs" :tcl="errorTcl" />

### `feather.registerType(interpId, typeName, typeDef)`

Registers a foreign object type. Use with `createForeign` to create instances that become callable commands:

<WasmPlayground :js="typeJs" :tcl="typeTcl" />

### `feather.destroy(interpId)`

Destroys an interpreter and frees resources.
