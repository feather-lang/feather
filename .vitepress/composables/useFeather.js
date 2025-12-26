import { ref, shallowRef } from 'vue'

let featherPromise = null
let sharedFeather = null

export function useFeather() {
  const ready = ref(false)
  const error = ref(null)
  const feather = shallowRef(null)

  if (!featherPromise) {
    featherPromise = (async () => {
      const { createFeather } = await import('../feather.js')
      const f = await createFeather('/feather.wasm')
      sharedFeather = f
      return f
    })()
  }

  featherPromise
    .then(f => {
      feather.value = f
      ready.value = true
    })
    .catch(e => {
      error.value = e?.message ?? String(e)
    })

  return { feather, ready, error }
}

export function createTestInterpreter(feather, opts = {}) {
  const interp = feather.create()
  
  // Register puts - captures to stdout
  feather.register(interp, 'puts', (args) => {
    opts.onStdout?.(args.join(' '))
    return ''
  })

  // Register echo - outputs args joined by space, returns empty (like tester.js)
  feather.register(interp, 'echo', (args) => {
    opts.onStdout?.(args.join(' '))
    return ''
  })

  // Register say-hello for test cases
  feather.register(interp, 'say-hello', () => {
    opts.onStdout?.('hello')
    return ''
  })

  // Register count - returns number of args
  feather.register(interp, 'count', (args) => {
    return String(args.length)
  })

  // Register list - returns args as Tcl list
  feather.register(interp, 'list', (args) => {
    const parts = args.map(s => {
      if (s.length === 0) return '{}'
      if (/[\s{}]/.test(s)) return '{' + s + '}'
      return s
    })
    return parts.join(' ')
  })

  // Register clock
  feather.register(interp, 'clock', (args) => {
    if (args[0] === 'seconds') return Math.floor(Date.now() / 1000)
    if (args[0] === 'milliseconds') return Date.now()
    throw new Error(`unknown clock subcommand "${args[0]}"`)
  })

  // Set up test variables referenced in test cases
  feather.eval(interp, 'set milestone m1')
  feather.eval(interp, 'set current-step m1')

  // Register Counter foreign type
  let nextCounterId = 1
  const counters = new Map()

  feather.registerType(interp, 'Counter', {
    methods: {
      get: (c) => c.value,
      set: (c, val) => { c.value = parseInt(val, 10); return '' },
      incr: (c) => ++c.value,
      add: (c, amount) => { c.value += parseInt(amount, 10); return c.value },
    },
    destroy: (c) => { counters.delete(c.id) }
  })

  feather.register(interp, 'Counter', (args) => {
    if (args.length === 0) {
      throw new Error('wrong # args: should be "Counter subcommand ?arg ...?"')
    }
    if (args[0] !== 'new') {
      throw new Error(`unknown subcommand "${args[0]}": must be new`)
    }
    const id = nextCounterId++
    const counter = { id, value: 0 }
    counters.set(id, counter)

    const handle = `counter${id}`
    feather.createForeign(interp, 'Counter', counter, handle)

    const methodDefs = {
      get: { argc: 0, fn: () => counter.value },
      set: { argc: 1, intArg: 1, fn: (val) => { counter.value = val; return '' } },
      incr: { argc: 0, fn: () => ++counter.value },
      add: { argc: 1, intArg: 1, fn: (amount) => { counter.value += amount; return counter.value } },
      destroy: { argc: 0, fn: () => {
        counters.delete(id)
        feather.destroyForeign(interp, handle)
        feather.register(interp, handle, () => { throw new Error(`invalid command name "${handle}"`) })
        return ''
      }}
    }

    feather.register(interp, handle, (methodArgs) => {
      if (methodArgs.length === 0) {
        throw new Error(`wrong # args: should be "${handle} method ?arg ...?"`)
      }
      const method = methodArgs[0]
      const rest = methodArgs.slice(1)
      const def = methodDefs[method]
      if (!def) {
        const methodList = Object.keys(methodDefs).join(', ')
        throw new Error(`unknown method "${method}": must be ${methodList}`)
      }
      if (rest.length !== def.argc) {
        throw new Error(`wrong # args: expected ${def.argc}, got ${rest.length}`)
      }
      const convertedArgs = rest.map((arg, idx) => {
        if (def.intArg === idx + 1) {
          const num = parseInt(arg, 10)
          if (isNaN(num)) {
            throw new Error(`argument ${idx + 1}: expected integer but got "${arg}"`)
          }
          return num
        }
        return arg
      })
      return String(def.fn(...convertedArgs) ?? '')
    })

    return handle
  })

  return interp
}
