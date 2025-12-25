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

  // Register clock
  feather.register(interp, 'clock', (args) => {
    if (args[0] === 'seconds') return Math.floor(Date.now() / 1000)
    if (args[0] === 'milliseconds') return Date.now()
    throw new Error(`unknown clock subcommand "${args[0]}"`)
  })

  // Set up test variables referenced in test cases
  feather.eval(interp, 'set milestone m1')
  feather.eval(interp, 'set current-step m1')

  return interp
}
