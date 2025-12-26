<template>
  <div class="wasm-playground">
    <div class="tabs">
      <button 
        :class="['tab', { active: activeTab === 'js' }]" 
        @click="activeTab = 'js'"
      >JavaScript</button>
      <button 
        :class="['tab', { active: activeTab === 'tcl' }]" 
        @click="activeTab = 'tcl'"
      >Feather</button>
      <div class="tab-spacer"></div>
      <button @click="runAll" :disabled="!ready" class="run-btn">▶ Run</button>
    </div>

    <div class="editor-container">
      <div v-show="activeTab === 'js'" class="editor-wrapper">
        <CodeEditor
          v-model="jsCode"
          language="javascript"
          :height="editorHeight"
        />
      </div>
      <div v-show="activeTab === 'tcl'" class="editor-wrapper">
        <CodeEditor
          v-model="tclCode"
          language="tcl"
          :height="editorHeight"
          @run="runAll"
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

    <div class="output-container">
      <div class="output-header">
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
  </div>
</template>

<script setup>
import { ref, onMounted, nextTick } from 'vue'
import CodeEditor from './CodeEditor.vue'

const props = defineProps({
  js: {
    type: String,
    default: ''
  },
  tcl: {
    type: String,
    default: ''
  },
  editorHeight: {
    type: String,
    default: '200px'
  }
})

const activeTab = ref('js')

const jsCode = ref('')
const tclCode = ref('')

onMounted(() => {
  jsCode.value = props.js || `// Register custom commands
register('greet', (args) => {
  return 'Hello, ' + (args[0] || 'World') + '!';
});`
  tclCode.value = props.tcl || `puts [greet "Feather"]`
})
const output = ref([])
const ready = ref(false)
const outputEl = ref(null)
const scriptError = ref(null)

let feather = null

function log(text, type = '') {
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

function clearError() {
  scriptError.value = null
}

function runAll() {
  if (!ready.value) return

  clearError()
  clearOutput()

  // Create fresh interpreter
  const interp = feather.create()

  // Register puts for output
  feather.register(interp, 'puts', (args) => {
    log(args.join(' '))
    return ''
  })

  // Execute JavaScript setup code
  try {
    const register = (name, fn) => {
      feather.register(interp, name, fn)
    }
    
    const fn = new Function('register', 'feather', 'interp', jsCode.value)
    fn(register, feather, interp)
  } catch (e) {
    scriptError.value = `JS Error: ${e.message}`
    log(`JavaScript error: ${e.message}`, 'error')
    feather.destroy(interp)
    return
  }

  // Execute Feather code
  try {
    const result = feather.eval(interp, tclCode.value)
    if (result) {
      log(`=> ${result}`, 'result')
    }
  } catch (e) {
    scriptError.value = e.message
    log(`Error: ${e.message}`, 'error')
  }

  feather.destroy(interp)
}

onMounted(async () => {
  try {
    const { createFeather } = await import('../feather.js')
    feather = await createFeather('/feather.wasm')
    ready.value = true
  } catch (e) {
    log(`Failed to load WASM: ${e.message}`, 'error')
    console.error(e)
  }
})
</script>

<style scoped>
.wasm-playground {
  margin: 24px 0;
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
  overflow: hidden;
}

.tabs {
  display: flex;
  align-items: center;
  background: var(--vp-c-bg-alt);
  border-bottom: 1px solid var(--vp-c-divider);
  padding: 0 8px;
}

.tab {
  background: none;
  border: none;
  padding: 10px 16px;
  font-size: 13px;
  font-weight: 500;
  color: var(--vp-c-text-2);
  cursor: pointer;
  border-bottom: 2px solid transparent;
  margin-bottom: -1px;
}

.tab:hover {
  color: var(--vp-c-text-1);
}

.tab.active {
  color: var(--vp-c-brand-1);
  border-bottom-color: var(--vp-c-brand-1);
}

.tab-spacer {
  flex: 1;
}

.editor-container {
  background: var(--vp-c-bg-soft);
}

.editor-wrapper {
  position: relative;
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

.run-btn {
  background: var(--vp-c-brand-1);
  color: white;
  border: none;
  padding: 6px 12px;
  border-radius: 4px;
  font-size: 12px;
  font-weight: 600;
  cursor: pointer;
  margin: 6px 0;
}

.run-btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.output-container {
  border-top: 1px solid var(--vp-c-divider);
}

.output-header {
  background: var(--vp-c-bg-alt);
  padding: 8px 12px;
  font-weight: 600;
  font-size: 13px;
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
  font-size: 13px;
  min-height: 60px;
  max-height: 120px;
  overflow-y: auto;
  background: var(--vp-c-bg-soft);
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
</style>
