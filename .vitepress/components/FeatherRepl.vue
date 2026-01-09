<template>
  <div class="repl">
    <div class="panel">
      <div class="panel-header">
        Feather REPL
        <button @click="clearOutput" class="clear-btn">Clear</button>
      </div>
      <div class="output" ref="outputEl">
        <div
          v-for="(line, i) in output"
          :key="i"
          :class="['output-line', line.type]"
        >{{ line.text }}</div>
      </div>
      <div class="input-container">
        <div v-if="completions.length > 0" class="completions" ref="completionsEl">
          <div
            v-for="(c, i) in completions"
            :key="i"
            :class="['completion-item', { selected: i === selectedCompletion }]"
            @click="applyCompletion(c)"
            @mouseenter="selectedCompletion = i"
          >
            <span class="completion-text">{{ formatCompletionText(c) }}</span>
            <span class="completion-type">{{ typeIndicator(c.type) }}</span>
            <span v-if="c.help" class="completion-help">{{ c.help }}</span>
          </div>
        </div>
        <div class="input-area">
          <span class="prompt">{{ inputBuffer ? '>' : '%' }}</span>
          <input
            ref="inputEl"
            v-model="inputText"
            @keydown="handleKeydown"
            :disabled="!ready"
            placeholder="Enter Feather command..."
            class="input"
            autocomplete="off"
            spellcheck="false"
          />
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, nextTick } from 'vue'

const output = ref([])
const inputText = ref('')
const ready = ref(false)
const outputEl = ref(null)
const inputEl = ref(null)
const completionsEl = ref(null)
const completions = ref([])
const selectedCompletion = ref(0)
const history = ref([])
const historyIndex = ref(-1)
const inputBuffer = ref('')  // Accumulates incomplete multiline input

let feather = null
let interp = null
let initialized = false

// Parse status constants
const TCL_PARSE_OK = 0
const TCL_PARSE_INCOMPLETE = 1
const TCL_PARSE_ERROR = 2

function log(text, type = 'output') {
  output.value.push({ text, type })
  nextTick(() => {
    if (outputEl.value) {
      outputEl.value.scrollTop = outputEl.value.scrollHeight
    }
  })
}

function clearOutput() {
  output.value = []
}

function typeIndicator(t) {
  switch (t) {
    case 'arg-placeholder': return 'A'
    case 'flag': return 'F'
    case 'command': return 'C'
    case 'subcommand': return 'S'
    case 'value': return 'V'
    default: return t ? t[0].toUpperCase() : '?'
  }
}

function formatCompletionText(c) {
  if (c.type === 'arg-placeholder' && c.name) {
    return `<${c.name}>`
  }
  return c.text
}

async function initFeather() {
  if (initialized) return
  initialized = true
  
  try {
    const { createFeather } = await import('../feather.js')
    feather = await createFeather('/feather.wasm')
    interp = feather.create()

    // Register puts
    feather.register(interp, 'puts', (args) => {
      log(args.join(' '))
      return ''
    })

    // Register clock
    feather.register(interp, 'clock', (args) => {
      if (args[0] === 'seconds') return Math.floor(Date.now() / 1000)
      if (args[0] === 'milliseconds') return Date.now()
      if (args[0] === 'clicks') return Math.floor(performance.now() * 1000)
      throw new Error(`unknown clock subcommand "${args[0]}"`)
    })

    // Register help command
    feather.register(interp, 'help', (args) => {
      if (args.length === 0) {
        // List all commands with usage specs
        const commands = feather.eval(interp, 'info commands')
        return `Available commands: ${commands}\n\nType "help <command>" for details, or press Tab for completion.`
      }
      const cmd = args[0]
      try {
        const helpText = feather.eval(interp, `usage help ${cmd}`)
        return helpText || `No help available for "${cmd}"`
      } catch (e) {
        return `No help available for "${cmd}"`
      }
    })

    ready.value = true
    log('Feather REPL ready. Type commands and press Enter.', 'info')
    log('Tab for completion, Shift+Tab to cycle back, Up/Down for history.', 'info')
    
    // Run help on startup
    try {
      const helpOutput = feather.eval(interp, 'help')
      if (helpOutput) {
        log(helpOutput, 'info')
      }
    } catch (e) {
      // Ignore errors
    }
  } catch (err) {
    log('Failed to load Feather: ' + err.message, 'error')
    console.error('Feather init error:', err)
  }
}

function runCommand(cmd) {
  if (!cmd.trim() && !inputBuffer.value) return

  // Build full script from buffer + current input
  const fullScript = inputBuffer.value ? inputBuffer.value + '\n' + cmd : cmd
  
  // Check if input is complete
  const parseResult = feather.parse(interp, fullScript)
  
  if (parseResult.status === TCL_PARSE_INCOMPLETE) {
    // Accumulate and continue
    const prompt = inputBuffer.value ? '> ' : '% '
    log(prompt + cmd, 'input')
    inputBuffer.value = fullScript
    return
  }
  
  if (parseResult.status === TCL_PARSE_ERROR) {
    // Show error and clear buffer
    const prompt = inputBuffer.value ? '> ' : '% '
    log(prompt + cmd, 'input')
    log('Error: ' + (parseResult.errorMessage || 'parse error'), 'error')
    inputBuffer.value = ''
    return
  }
  
  // Complete input - add to history and evaluate
  if (history.value[history.value.length - 1] !== fullScript) {
    history.value.push(fullScript)
  }
  historyIndex.value = history.value.length

  const prompt = inputBuffer.value ? '> ' : '% '
  log(prompt + cmd, 'input')
  
  try {
    const result = feather.eval(interp, fullScript)
    if (result !== '' && result !== undefined) {
      log(String(result), 'result')
    }
  } catch (err) {
    log('Error: ' + err.message, 'error')
  }
  
  inputBuffer.value = ''
}

function getCompletions() {
  if (!ready.value || !inputText.value) {
    completions.value = []
    return
  }

  try {
    // Build full script including accumulated multiline input
    const currentLine = inputText.value
    const fullScript = inputBuffer.value ? inputBuffer.value + '\n' + currentLine : currentLine
    const pos = inputBuffer.value 
      ? inputBuffer.value.length + 1 + (inputEl.value?.selectionStart || currentLine.length)
      : (inputEl.value?.selectionStart || currentLine.length)
    
    // Use call API to pass script directly without escaping
    const result = feather.call(interp, 'usage', 'complete', fullScript, pos)
    
    if (result) {
      const parsed = parseCompletionList(result)
      completions.value = parsed
      selectedCompletion.value = 0
    } else {
      completions.value = []
    }
  } catch (err) {
    console.error('Completion error:', err)
    completions.value = []
  }
}

function parseCompletionList(result) {
  if (!result || result === '') return []
  
  try {
    const len = parseInt(feather.eval(interp, `llength {${result}}`))
    const items = []
    
    for (let i = 0; i < len; i++) {
      const dict = feather.eval(interp, `lindex {${result}} ${i}`)
      
      const item = {
        text: feather.eval(interp, `dict get {${dict}} text`) || '',
        type: feather.eval(interp, `dict get {${dict}} type`) || '',
        help: feather.eval(interp, `dict get {${dict}} help`) || ''
      }
      
      if (item.type === 'arg-placeholder') {
        item.name = feather.eval(interp, `dict get {${dict}} name`) || ''
      }
      
      items.push(item)
    }
    
    return items
  } catch (err) {
    return []
  }
}

function applyCompletion(c) {
  if (c.type === 'arg-placeholder') {
    // Don't insert placeholders, just close popup
    completions.value = []
    return
  }
  
  const input = inputText.value
  const pos = inputEl.value?.selectionStart || input.length
  
  // Find the start of the current word
  let wordStart = pos
  while (wordStart > 0 && !isWordBreak(input[wordStart - 1])) {
    wordStart--
  }
  
  // Replace current word with completion + space
  const before = input.slice(0, wordStart)
  const after = input.slice(pos)
  inputText.value = before + c.text + ' ' + after
  
  completions.value = []
  
  nextTick(() => {
    const newPos = wordStart + c.text.length + 1
    inputEl.value?.setSelectionRange(newPos, newPos)
    inputEl.value?.focus()
  })
}

function isWordBreak(ch) {
  return ' \t;{}'.includes(ch)
}

function handleKeydown(e) {
  // Tab / Shift+Tab for completion cycling
  if (e.key === 'Tab') {
    e.preventDefault()
    
    if (completions.value.length > 0) {
      // Cycle through completions
      if (e.shiftKey) {
        selectedCompletion.value = (selectedCompletion.value - 1 + completions.value.length) % completions.value.length
      } else {
        selectedCompletion.value = (selectedCompletion.value + 1) % completions.value.length
      }
      scrollCompletionIntoView()
    } else {
      // Get completions
      getCompletions()
      if (e.shiftKey && completions.value.length > 0) {
        selectedCompletion.value = completions.value.length - 1
      }
    }
    return
  }
  
  // Enter to apply completion or run command
  if (e.key === 'Enter') {
    e.preventDefault()
    if (completions.value.length > 0) {
      applyCompletion(completions.value[selectedCompletion.value])
    } else {
      runCommand(inputText.value)
      inputText.value = ''
    }
    return
  }
  
  // Arrow keys for popup navigation
  if (completions.value.length > 0) {
    if (e.key === 'ArrowDown') {
      e.preventDefault()
      selectedCompletion.value = (selectedCompletion.value + 1) % completions.value.length
      scrollCompletionIntoView()
      return
    }
    if (e.key === 'ArrowUp') {
      e.preventDefault()
      selectedCompletion.value = (selectedCompletion.value - 1 + completions.value.length) % completions.value.length
      scrollCompletionIntoView()
      return
    }
  }
  
  // Escape to close popup or cancel multiline input
  if (e.key === 'Escape') {
    e.preventDefault()
    if (completions.value.length > 0) {
      completions.value = []
    } else if (inputBuffer.value) {
      // Cancel multiline input
      log('Incomplete input, discarded', 'info')
      inputBuffer.value = ''
      inputText.value = ''
    }
    return
  }
  
  // Ctrl-C to cancel multiline input
  if (e.key === 'c' && e.ctrlKey) {
    if (inputBuffer.value) {
      e.preventDefault()
      log('Incomplete input, discarded', 'info')
      inputBuffer.value = ''
      inputText.value = ''
      return
    }
  }
  
  // History navigation (only when popup is closed)
  if (completions.value.length === 0) {
    if (e.key === 'ArrowUp') {
      e.preventDefault()
      if (historyIndex.value > 0) {
        historyIndex.value--
        inputText.value = history.value[historyIndex.value]
      }
      return
    }
    
    if (e.key === 'ArrowDown') {
      e.preventDefault()
      if (historyIndex.value < history.value.length - 1) {
        historyIndex.value++
        inputText.value = history.value[historyIndex.value]
      } else {
        historyIndex.value = history.value.length
        inputText.value = ''
      }
      return
    }
  }
  
  // Close popup on any other key
  if (completions.value.length > 0 && !['Shift', 'Control', 'Alt', 'Meta'].includes(e.key)) {
    completions.value = []
  }
}

function scrollCompletionIntoView() {
  nextTick(() => {
    const el = completionsEl.value?.children[selectedCompletion.value]
    if (el) {
      el.scrollIntoView({ block: 'nearest' })
    }
  })
}

onMounted(() => {
  initFeather()
})
</script>

<style scoped>
.repl {
  margin: 16px 0;
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

.clear-btn {
  background: var(--vp-c-bg-soft);
  color: var(--vp-c-text-1);
  border: 1px solid var(--vp-c-divider);
  padding: 4px 10px;
  border-radius: 4px;
  font-size: 12px;
  cursor: pointer;
}

.output {
  padding: 12px;
  font-family: var(--vp-font-family-mono);
  font-size: 14px;
  min-height: 150px;
  max-height: 300px;
  overflow-y: auto;
  line-height: 1.5;
}

.output > div {
  margin: 0 !important;
  padding: 0 !important;
}

.output-line {
  display: block;
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

.output-line.input {
  color: var(--vp-c-brand-1);
}

.input-container {
  position: relative;
}

.input-area {
  display: flex;
  align-items: center;
  border-top: 1px solid var(--vp-c-divider);
  background: var(--vp-c-bg-alt);
}

.prompt {
  padding: 12px;
  font-family: var(--vp-font-family-mono);
  font-size: 14px;
  font-weight: 600;
  color: var(--vp-c-brand-1);
}

.input {
  flex: 1;
  background: transparent;
  border: none;
  padding: 12px 12px 12px 0;
  font-family: var(--vp-font-family-mono);
  font-size: 14px;
  color: var(--vp-c-text-1);
  outline: none;
}

.input::placeholder {
  color: var(--vp-c-text-3);
}

.completions {
  position: absolute;
  bottom: 100%;
  left: 0;
  right: 0;
  background: var(--vp-c-bg);
  border: 1px solid var(--vp-c-divider);
  border-bottom: none;
  max-height: 200px;
  overflow-y: auto;
  box-shadow: 0 -4px 12px rgba(0, 0, 0, 0.15);
}

.completion-item {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 6px 12px;
  cursor: pointer;
  font-family: var(--vp-font-family-mono);
  font-size: 13px;
}

.completion-item:hover,
.completion-item.selected {
  background: var(--vp-c-brand-soft);
}

.completion-text {
  font-weight: 500;
  color: var(--vp-c-text-1);
  min-width: 100px;
}

.completion-type {
  color: var(--vp-c-text-3);
  font-size: 11px;
  padding: 1px 5px;
  background: var(--vp-c-bg-alt);
  border-radius: 3px;
  font-weight: 600;
}

.completion-help {
  flex: 1;
  color: var(--vp-c-text-2);
  font-size: 12px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
</style>
