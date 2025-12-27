<template>
  <div :class="['playground', { compact: isCompact }]">
    <template v-if="!isCompact">
      <h2>Interactive Playground</h2>
      <p class="subtitle">Try Feather directly in your browser - powered by WebAssembly</p>

      <div class="examples">
        <button
          v-for="ex in examples"
          :key="ex.name"
          @click="loadExample(ex)"
          class="example-btn"
        >{{ ex.name }}</button>
      </div>
    </template>

    <div class="editor-container">
      <div class="panel editor-panel">
        <div class="panel-header">
          Feather Script
          <button @click="runScript" :disabled="!ready" class="run-btn">▶ Run</button>
        </div>
        <div class="editor-wrapper">
          <CodeEditor
            v-model="script"
            language="tcl"
            @run="runScript"
          />
          <div v-if="scriptError" class="error-overlay" @click="clearError">
            <div class="error-content">
              <div class="error-title">Error</div>
              <div class="error-message">{{ scriptError }}</div>
              <div class="error-hint">Click to dismiss</div>
            </div>
          </div>
        </div>
      </div>

      <div class="panel" v-if="showHostCode">
        <div class="panel-header">
          Host Code (JavaScript)
          <button @click="registerHostCode" :disabled="!ready" class="register-btn">+ Register</button>
        </div>
        <CodeEditor
          v-model="hostCode"
          language="javascript"
        />
      </div>

      <div class="panel" v-else-if="!showCanvas && !isCompact">
        <div class="panel-header">
          Output
          <button @click="clearOutput" class="clear-btn">Clear</button>
        </div>
        <div class="output" ref="outputEl">
          <div
            v-for="(line, i) in output"
            :key="i"
            :class="['output-line', line.type]"
          >{{ line.text }}</div>
        </div>
      </div>

      <div class="panel canvas-panel" v-else-if="showCanvas && !isCompact">
        <div class="panel-header">Canvas</div>
        <canvas ref="canvasEl" width="800" height="300"></canvas>
      </div>
    </div>

    <div class="panel output-panel" v-if="showHostCode && !showCanvas">
      <div class="panel-header">
        Output
        <button @click="clearOutput" class="clear-btn">Clear</button>
      </div>
      <div class="output" ref="outputEl">
        <div
          v-for="(line, i) in output"
          :key="i"
          :class="['output-line', line.type]"
        >{{ line.text }}</div>
      </div>
    </div>

    <div class="panel output-panel compact-output" v-if="isCompact && !showCanvas">
      <div class="panel-header">
        Output
        <button @click="clearOutput" class="clear-btn">Clear</button>
      </div>
      <div class="output" ref="compactOutputEl">
        <div
          v-for="(line, i) in output"
          :key="i"
          :class="['output-line', line.type]"
        >{{ line.text }}</div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, nextTick, useSlots } from 'vue'
import CodeEditor from './CodeEditor.vue'

const props = defineProps({
  code: {
    type: String,
    default: null
  }
})

const slots = useSlots()

const defaultCode = `# Pulsing circle animation
set state [dict create]
dict set state radius 20
dict set state growing 1
dict set state min 20
dict set state max 100

proc grow {} {
    upvar #0 state s
    dict incr s radius
    if {[dict get $s radius] >= [dict get $s max]} {
        dict set s growing 0
    }
}

proc shrink {} {
    upvar #0 state s
    dict incr s radius -1
    if {[dict get $s radius] <= [dict get $s min]} {
        dict set s growing 1
    }
}

proc update_radius {} {
    upvar #0 state s
    if {[dict get $s growing]} { grow } else { shrink }
}

proc draw {} {
    upvar #0 state s
    update_radius
    set r [dict get $s radius]
    canvas clear
    canvas fill #e94560
    canvas circle 250 140 $r
}

animate draw`

const isCompact = computed(() => props.code !== null || slots.default)

// Get code from slot or prop
function extractTextFromVNodes(vnodes) {
  let text = ''
  for (const vnode of vnodes) {
    if (typeof vnode === 'string') {
      text += vnode
    } else if (typeof vnode.children === 'string') {
      text += vnode.children
    } else if (Array.isArray(vnode.children)) {
      text += extractTextFromVNodes(vnode.children)
    }
  }
  return text
}

function getInitialCode() {
  if (props.code) return props.code
  if (slots.default) {
    const slotContent = slots.default()
    if (slotContent) {
      const text = extractTextFromVNodes(slotContent)
      if (text) return text.trim()
    }
  }
  return defaultCode
}

const script = ref(getInitialCode())

const hostCode = ref('')
const showHostCode = ref(false)
const showCanvas = ref(props.code === null && !slots.default)

const output = ref([])
const ready = ref(false)
const outputEl = ref(null)
const compactOutputEl = ref(null)
const canvasEl = ref(null)
const scriptError = ref(null)
const persistentInterp = ref(false)

let feather = null
let interp = null
let animationFrameId = null
let animationStartTime = null

const examples = [
  { name: 'Canvas', hostCode: null, code: `# Pulsing circle animation
set state [dict create]
dict set state radius 20
dict set state growing 1
dict set state min 20
dict set state max 100

proc grow {} {
    upvar #0 state s
    dict incr s radius
    if {[dict get $s radius] >= [dict get $s max]} {
        dict set s growing 0
    }
}

proc shrink {} {
    upvar #0 state s
    dict incr s radius -1
    if {[dict get $s radius] <= [dict get $s min]} {
        dict set s growing 1
    }
}

proc update_radius {} {
    upvar #0 state s
    if {[dict get $s growing]} { grow } else { shrink }
}

proc draw {} {
    upvar #0 state s
    update_radius
    set r [dict get $s radius]
    canvas clear
    canvas fill #e94560
    canvas circle 250 140 $r
}

animate draw` },
  { name: 'Hello World', hostCode: null, code: `puts "Hello, World!"
set name "Feather"
puts "Welcome to $name!"` },
  { name: 'Control Structures', hostCode: null, code: `# Define a custom "repeat" control structure
proc repeat {n body} {
    for {set i 0} {$i < $n} {incr i} {
        uplevel 1 $body
    }
}

# Use it like a built-in command
repeat 3 {
    puts "Hello!"
}

# Define "unless" (opposite of if)
proc unless {cond body} {
    if {!$cond} {
        uplevel 1 $body
    }
}

set debug 0
unless $debug {
    puts "Debug mode is off"
}

# Define "with" for setup/teardown
proc with {setup teardown body} {
    uplevel 1 $setup
    set result [uplevel 1 $body]
    uplevel 1 $teardown
    return $result
}

with {puts "Starting..."} {puts "Done!"} {
    puts "Doing work"
}` },
  { name: 'Introspection', hostCode: null, code: `# Feather lets you inspect everything at runtime

# Define some procedures
proc greet {name} { return "Hello, $name!" }
proc add {a b} { expr {$a + $b} }

# List all user-defined procedures
puts "Procedures: [info procs]"

# Inspect a procedure's parameters
puts "greet args: [info args greet]"
puts "add args: [info args add]"

# Inspect a procedure's body
puts "greet body: [info body greet]"

# Check if commands exist
puts "greet exists: [info commands greet]"
puts "foo exists: [info commands foo]"

# Inspect variables in current scope
set x 10
set y 20
set name "Feather"
puts "Variables: [info vars]"

# Check if a variable exists
puts "x exists: [info exists x]"
puts "z exists: [info exists z]"

# Get the call stack level
proc inner {} {
    puts "Call level: [info level]"
}
proc outer {} { inner }
outer` },
  { name: 'Host Functions', persistent: true, hostCode: `// Expose JavaScript functions to Feather
// Click "+ Register" to add them

register("time", (args) => {
  const fmt = args[0] || "human";
  const now = new Date();
  if (fmt === "unix") return Math.floor(now.getTime() / 1000);
  return now.toLocaleString();
});`, code: `# First click "+ Register" to load the host code!

# Call the JavaScript time function
puts "Current time: [time]"
puts "Unix timestamp: [time unix]"` },
]

function log(text, type = '') {
  output.value.push({ text, type })
  nextTick(() => {
    if (outputEl.value) {
      outputEl.value.scrollTop = outputEl.value.scrollHeight
    }
    if (compactOutputEl.value) {
      compactOutputEl.value.scrollTop = compactOutputEl.value.scrollHeight
    }
  })
}

function stopAnimation() {
  if (animationFrameId) {
    cancelAnimationFrame(animationFrameId)
    animationFrameId = null
  }
}

function loadExample(ex) {
  stopAnimation()
  script.value = ex.code
  showCanvas.value = ex.name === 'Canvas'
  persistentInterp.value = ex.persistent || false
  if (ex.hostCode) {
    hostCode.value = ex.hostCode
    showHostCode.value = true
  } else {
    hostCode.value = ''
    showHostCode.value = false
  }
  // Reset interpreter when switching examples
  if (feather) {
    interp = feather.create()
    registerBuiltinCommands()
  }
}

function clearOutput() {
  output.value = []
}

function registerHostCode() {
  if (!ready.value || !hostCode.value.trim()) return

  log('--- Registering host functions ---', 'info')

  try {
    // Create a register function that the user code can call
    const register = (name, fn) => {
      feather.register(interp, name, fn)
      log(`Registered command: ${name}`, 'result')
    }

    // Execute the user's JavaScript code
    const fn = new Function('register', hostCode.value)
    fn(register)

    log('Host functions registered successfully!', 'result')
  } catch (e) {
    log(`Error: ${e.message}`, 'error')
  }
}

function registerBuiltinCommands() {
  feather.register(interp, 'puts', (args) => {
    log(args.join(' '))
    return ''
  })

  feather.register(interp, 'clock', (args) => {
    if (args[0] === 'seconds') return Math.floor(Date.now() / 1000)
    if (args[0] === 'milliseconds') return Date.now()
    throw new Error(`unknown clock subcommand "${args[0]}"`)
  })

  // Canvas commands
  let ctx = null
  let logicalWidth = 800
  let logicalHeight = 300

  function setupCanvas() {
    const canvas = canvasEl.value
    if (!canvas) return
    ctx = canvas.getContext('2d')
    const dpr = window.devicePixelRatio || 1
    const parent = canvas.parentElement
    logicalWidth = parent.clientWidth
    const header = parent.querySelector('.panel-header')
    const headerHeight = header ? header.offsetHeight : 0
    logicalHeight = Math.max(parent.clientHeight - headerHeight, 280)
    canvas.width = logicalWidth * dpr
    canvas.height = logicalHeight * dpr
    canvas.style.width = logicalWidth + 'px'
    canvas.style.height = logicalHeight + 'px'
    ctx.scale(dpr, dpr)
  }

  setupCanvas()

  feather.register(interp, 'canvas', (args) => {
    if (!ctx) setupCanvas()
    if (!ctx) throw new Error('canvas not available')
    const sub = args[0]
    switch (sub) {
      case 'clear':
        ctx.clearRect(0, 0, logicalWidth, logicalHeight)
        break
      case 'fill':
        ctx.fillStyle = args[1]
        break
      case 'stroke':
        ctx.strokeStyle = args[1]
        break
      case 'width':
        ctx.lineWidth = parseFloat(args[1])
        break
      case 'rect':
        ctx.fillRect(parseFloat(args[1]), parseFloat(args[2]), parseFloat(args[3]), parseFloat(args[4]))
        break
      case 'circle':
        ctx.beginPath()
        ctx.arc(parseFloat(args[1]), parseFloat(args[2]), parseFloat(args[3]), 0, Math.PI * 2)
        ctx.fill()
        break
      case 'line':
        ctx.beginPath()
        ctx.moveTo(parseFloat(args[1]), parseFloat(args[2]))
        ctx.lineTo(parseFloat(args[3]), parseFloat(args[4]))
        ctx.stroke()
        break
      case 'polygon': {
        const points = args.slice(1).map(parseFloat)
        if (points.length >= 4) {
          ctx.beginPath()
          ctx.moveTo(points[0], points[1])
          for (let i = 2; i < points.length; i += 2) {
            ctx.lineTo(points[i], points[i + 1])
          }
          ctx.closePath()
          ctx.fill()
        }
        break
      }
      case 'font':
        ctx.font = args[1]
        break
      case 'text':
        ctx.fillText(args.slice(3).join(' '), parseFloat(args[1]), parseFloat(args[2]))
        break
      default:
        throw new Error(`unknown canvas subcommand "${sub}"`)
    }
    return ''
  })

  // Register 'animate' command - runs for 30 seconds max
  const ANIMATION_DURATION_MS = 5000
  feather.register(interp, 'animate', (args) => {
    const procName = args[0] || 'draw'
    stopAnimation()
    animationStartTime = Date.now()
    
    function animationLoop() {
      const elapsed = Date.now() - animationStartTime
      if (elapsed >= ANIMATION_DURATION_MS) {
        const seconds = Math.round(elapsed / 1000)
        if (ctx) {
          ctx.font = '14px monospace'
          ctx.fillStyle = 'white'
          ctx.fillText(`Animation stopped after ${seconds} seconds`, 10, 20)
        }
        log('Animation stopped (30s limit)', 'info')
        stopAnimation()
        return
      }
      
      try {
        feather.eval(interp, procName)
        animationFrameId = requestAnimationFrame(animationLoop)
      } catch (e) {
        const errMsg = e.message || feather.getResult(interp) || JSON.stringify(e)
        scriptError.value = errMsg
        log(`Animation error: ${errMsg}`, 'error')
        stopAnimation()
      }
    }
    animationFrameId = requestAnimationFrame(animationLoop)
    return ''
  })
}

function clearError() {
  scriptError.value = null
}

function runScript() {
  if (!ready.value || !script.value.trim()) return

  stopAnimation()
  clearError()
  
  // Create fresh interpreter for each run, unless persistent mode is enabled
  if (!persistentInterp.value) {
    interp = feather.create()
    registerBuiltinCommands()
  }
  
  log('--- Running script ---', 'info')

  try {
    const result = feather.eval(interp, script.value)
    if (result) {
      log(`=> ${result}`, 'result')
    }
  } catch (e) {
    const errMsg = e.message || feather.getResult?.(interp) || String(e)
    scriptError.value = errMsg
    log(`Error: ${errMsg}`, 'error')
  }
}

onMounted(async () => {
  try {
    const { createFeather } = await import('../feather.js')
    feather = await createFeather('/feather.wasm')
    interp = feather.create()
    registerBuiltinCommands()

    ready.value = true
    log('Feather interpreter ready', 'info')
  } catch (e) {
    log(`Failed to load WASM: ${e.message}`, 'error')
    console.error(e)
  }
})
</script>

<style scoped>
.playground {
  margin: 40px 0;
}

.playground.compact {
  margin: 16px 0;
}

.playground.compact .editor-container {
  grid-template-columns: 1fr;
}

.playground.compact .panel textarea,
.playground.compact .output {
  min-height: 80px;
  max-height: 150px;
}

.playground.compact .compact-output {
  margin-top: 0;
}

.playground.compact .compact-output .output {
  min-height: 60px;
  max-height: 120px;
}

.playground h2 {
  margin: 0 0 8px 0;
  border: none;
}

.subtitle {
  color: var(--vp-c-text-2);
  margin-bottom: 20px;
}

.run-btn {
  background: var(--vp-c-brand-1);
  color: white;
  border: none;
  padding: 4px 10px;
  border-radius: 4px;
  font-size: 12px;
  font-weight: 600;
  cursor: pointer;
}

.run-btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.clear-btn {
  background: var(--vp-c-bg-soft);
  color: var(--vp-c-text-1);
  border: 1px solid var(--vp-c-divider);
  padding: 4px 10px;
  border-radius: 4px;
  font-size: 12px;
  cursor: pointer;
}

.editor-panel {
  display: flex;
  flex-direction: column;
}

.editor-wrapper {
  position: relative;
  flex: 1;
}

.error-overlay {
  position: absolute;
  inset: 0;
  background: rgba(0, 0, 0, 0.6);
  backdrop-filter: blur(4px);
  display: flex;
  align-items: center;
  justify-content: center;
  cursor: pointer;
  z-index: 10;
}

.error-content {
  background: rgba(239, 68, 68, 0.9);
  color: white;
  padding: 16px 24px;
  border-radius: 8px;
  max-width: 80%;
  text-align: center;
}

.error-title {
  font-weight: 700;
  font-size: 14px;
  margin-bottom: 8px;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

.error-message {
  font-family: var(--vp-font-family-mono);
  font-size: 13px;
  white-space: pre-wrap;
  word-break: break-word;
}

.error-hint {
  margin-top: 12px;
  font-size: 11px;
  opacity: 0.7;
}

.editor-container {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 16px;
  margin-bottom: 16px;
}

@media (max-width: 768px) {
  .editor-container {
    grid-template-columns: 1fr;
  }
}

.panel {
  background: var(--vp-c-bg-soft);
  border-radius: 8px;
  overflow: hidden;
  border: 1px solid var(--vp-c-divider);
}

.panel-header {
  background: var(--vp-c-bg-alt);
  padding: 8px 12px;
  font-weight: 600;
  font-size: 14px;
  border-bottom: 1px solid var(--vp-c-divider);
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.register-btn {
  background: var(--vp-c-brand-1);
  color: white;
  border: none;
  padding: 4px 10px;
  border-radius: 4px;
  font-size: 12px;
  font-weight: 600;
  cursor: pointer;
}

.register-btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.panel textarea {
  width: 100%;
  height: 280px;
  background: var(--vp-c-bg-soft);
  color: var(--vp-c-text-1);
  border: none;
  padding: 12px;
  font-family: var(--vp-font-family-mono);
  font-size: 14px;
  resize: vertical;
  outline: none;
}

.host-textarea {
  height: 280px;
}

.output-panel {
  margin-bottom: 16px;
}

.output-panel .output {
  min-height: 120px;
  max-height: 120px;
}

.output {
  padding: 12px;
  font-family: var(--vp-font-family-mono);
  font-size: 14px;
  min-height: 280px;
  max-height: 280px;
  overflow-y: auto;
}

.output-line {
  margin: 2px 0;
  white-space: pre-wrap;
  word-break: break-all;
}

.output-line.error {
  color: #ef4444;
}

.output-line.result {
  color: #4ade80;
}

.output-line.info {
  color: var(--vp-c-text-3);
}

.examples {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 8px;
  margin-bottom: 16px;
}

.example-btn {
  background: var(--vp-c-bg-soft);
  color: var(--vp-c-text-1);
  border: 1px solid var(--vp-c-divider);
  padding: 4px 10px;
  border-radius: 4px;
  font-size: 12px;
  cursor: pointer;
}

.example-btn:hover {
  border-color: var(--vp-c-brand-1);
}

.canvas-panel {
  display: flex;
  flex-direction: column;
}

.canvas-panel canvas {
  flex: 1;
  width: 100%;
  background: #111;
  display: block;
}
</style>
