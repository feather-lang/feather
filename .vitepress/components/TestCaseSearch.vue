<template>
  <div class="test-search">
    <div class="search-box">
      <input
        v-model="query"
        type="text"
        placeholder="Search test cases by name or script..."
        class="search-input"
      />
      <span class="result-count" v-if="query">
        {{ filteredTests.length }} of {{ allTests.length }}
      </span>
    </div>

    <div v-if="parseError" class="parse-error">Failed to parse tests: {{ parseError }}</div>
    <div v-else-if="!ready" class="loading">Loading Feather interpreter...</div>

    <div class="results" v-if="query && filteredTests.length > 0">
      <div
        v-for="(tc, i) in displayedTests"
        :key="tc.name + '-' + i"
        class="test-case"
        :class="tc.status"
      >
        <div class="test-header" @click="toggleExpanded(tc)">
          <div class="test-title">
            <span class="status-icon">
              <span v-if="tc.status === 'pass'">✓</span>
              <span v-else-if="tc.status === 'fail'">✗</span>
              <span v-else-if="tc.status === 'running'">⏳</span>
              <span v-else>○</span>
            </span>
            <span class="name" v-html="highlight(tc.name)"></span>
          </div>
          <div class="test-actions" @click.stop>
            <button class="run-btn" @click="runTest(tc)" :disabled="!ready">Run</button>
          </div>
        </div>

        <div class="test-body" v-show="tc.expanded">
          <div class="editor-section">
            <div class="section-label">Script</div>
            <CodeEditor v-model="tc.script" language="tcl" :minHeight="60" />
          </div>

          <div class="results-section" v-if="tc.lastRun">
            <div class="result-columns">
              <div class="result-column">
                <div class="column-header">Expected</div>
                <div class="result-field" :class="fieldClass(tc.fieldStatus.stdout)" v-if="tc.expected.stdoutSet">
                  <span class="field-label">stdout:</span>
                  <pre>{{ tc.expected.stdout || '(empty)' }}</pre>
                </div>
                <div class="result-field" :class="fieldClass(tc.fieldStatus.error)" v-if="tc.expected.error">
                  <span class="field-label">error:</span>
                  <pre>{{ tc.expected.error || '(empty)' }}</pre>
                </div>
              </div>
              <div class="result-column">
                <div class="column-header">Actual</div>
                <div class="result-field" :class="fieldClass(tc.fieldStatus.stdout)" v-if="tc.expected.stdoutSet">
                  <span class="field-label">stdout:</span>
                  <pre>{{ tc.lastRun.stdout || '(empty)' }}</pre>
                </div>
                <div class="result-field" :class="fieldClass(tc.fieldStatus.error)" v-if="tc.expected.error">
                  <span class="field-label">error:</span>
                  <pre>{{ tc.lastRun.error || '(empty)' }}</pre>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>

      <button class="more-results" v-if="filteredTests.length > displayLimit" @click="showMore">
        + {{ filteredTests.length - displayLimit }} more results
      </button>
    </div>

    <div class="no-results" v-else-if="query && filteredTests.length === 0">
      No test cases match "{{ query }}"
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, watch } from 'vue'
import CodeEditor from './CodeEditor.vue'
import { useFeather, createTestInterpreter, resetFeather } from '../composables/useFeather.js'
import { TCL_OK, TCL_ERROR, TCL_RETURN, TCL_BREAK, TCL_CONTINUE } from '../feather.js'

const props = defineProps({
  source: { type: String, required: true },
  maxResults: { type: Number, default: 10 }
})

const { feather, ready, error: wasmError } = useFeather()
const allTests = ref([])
const parseError = ref(null)
const query = ref('')
const displayLimit = ref(props.maxResults)

function setQuery(q) {
  query.value = q
  displayLimit.value = props.maxResults
}

function showMore() {
  displayLimit.value += 10
}

defineExpose({ setQuery })

const RETURN_CODE_NAMES = {
  [TCL_OK]: 'TCL_OK',
  [TCL_ERROR]: 'TCL_ERROR',
  [TCL_RETURN]: 'TCL_RETURN',
  [TCL_BREAK]: 'TCL_BREAK',
  [TCL_CONTINUE]: 'TCL_CONTINUE',
}

const filteredTests = computed(() => {
  if (!query.value.trim()) return []
  const q = query.value.toLowerCase()
  return allTests.value.filter(tc => 
    tc.name.toLowerCase().includes(q) || 
    tc.script.toLowerCase().includes(q)
  )
})

const displayedTests = computed(() => {
  return filteredTests.value.slice(0, displayLimit.value)
})

function highlight(text) {
  if (!query.value.trim()) return text
  const q = query.value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')
  const regex = new RegExp(`(${q})`, 'gi')
  return text.replace(regex, '<mark>$1</mark>')
}

function normalizeLines(content) {
  if (!content) return ''
  const lines = content.split('\n')
  for (let i = 0; i < lines.length; i++) {
    lines[i] = lines[i].trim()
  }
  let start = 0
  while (start < lines.length && lines[start] === '') start++
  let end = lines.length
  while (end > start && lines[end - 1] === '') end--
  if (start >= end) return ''
  return lines.slice(start, end).join('\n')
}

// For scripts: only strip leading/trailing empty lines, preserve internal whitespace
function normalizeScript(content) {
  if (!content) return ''
  const lines = content.split('\n')
  let start = 0
  while (start < lines.length && lines[start].trim() === '') start++
  let end = lines.length
  while (end > start && lines[end - 1].trim() === '') end--
  if (start >= end) return ''
  return lines.slice(start, end).join('\n')
}

function decodeHtmlEntities(text) {
  const textarea = document.createElement('textarea')
  textarea.innerHTML = text
  return textarea.value
}

function parseTestSuite(source) {
  try {
    const parser = new DOMParser()
    const doc = parser.parseFromString(source, 'text/html')
    const caseEls = Array.from(doc.querySelectorAll('test-case'))

    return caseEls.map(el => {
      const getField = (tag) => {
        const child = el.querySelector(tag)
        if (!child) return { present: false, value: undefined }
        const raw = decodeHtmlEntities(child.textContent ?? '')
        return { present: true, value: normalizeLines(raw) }
      }

      const name = el.getAttribute('name') ?? ''
      const scriptRaw = el.querySelector('script')?.textContent ?? ''
      const script = normalizeScript(decodeHtmlEntities(scriptRaw))
      
      const returnField = getField('return')
      const resultField = getField('result')
      const errorField = getField('error')
      const stdoutField = getField('stdout')
      const stderrField = getField('stderr')
      const exitCodeField = getField('exit-code')

      return {
        name,
        originalScript: script,
        script,
        expected: {
          returnCode: returnField.value,
          result: resultField.value,
          error: errorField.value,
          stdout: stdoutField.value,
          stdoutSet: stdoutField.present,
          stderr: stderrField.value,
          exitCode: exitCodeField.present && exitCodeField.value !== ''
            ? parseInt(exitCodeField.value, 10)
            : undefined,
        },
        lastRun: null,
        status: 'idle',
        fieldStatus: {},
        expanded: false
      }
    })
  } catch (e) {
    parseError.value = e?.message ?? String(e)
    return []
  }
}

function fieldClass(status) {
  return { pass: status === true, fail: status === false }
}

function toggleExpanded(tc) {
  tc.expanded = !tc.expanded
}

async function runTest(tc) {
  if (!ready.value) return

  tc.status = 'running'
  tc.lastRun = null
  tc.fieldStatus = {}
  tc.expanded = true

  const stdout = []
  const stderr = []

  // Reset WASM module to get fresh state
  feather.value = await resetFeather()

  try {
    const interp = createTestInterpreter(feather.value, {
      onStdout: line => stdout.push(line),
      onStderr: line => stderr.push(line),
    })

    let returnCode = 'TCL_OK'
    let resultStr = ''
    let errorStr = ''
    let exitCode = 0

    const parseResult = feather.value.parse(interp, tc.script)
    
    if (parseResult.status === 1) {
      returnCode = 'TCL_OK'
      resultStr = parseResult.result
      exitCode = 2
    } else if (parseResult.status === 2) {
      returnCode = 'TCL_ERROR'
      resultStr = parseResult.result
      errorStr = parseResult.errorMessage || ''
      exitCode = 3
    } else {
      try {
        const evalResult = feather.value.eval(interp, tc.script)
        resultStr = evalResult || ''
        if (evalResult !== '') stdout.push(evalResult)
        returnCode = 'TCL_OK'
        exitCode = 0
      } catch (e) {
        errorStr = e.message || ''
        returnCode = 'TCL_ERROR'
        exitCode = 1
      }
    }

    const actual = {
      returnCode,
      result: resultStr,
      error: normalizeLines(errorStr),
      stdout: normalizeLines(stdout.join('\n')),
      stderr: normalizeLines(stderr.join('\n')),
      exitCode,
    }

    tc.lastRun = actual

    const fs = {}
    const ex = tc.expected

    if (ex.stdoutSet) fs.stdout = (ex.stdout ?? '') === actual.stdout
    fs.stderr = (ex.stderr ?? '') === actual.stderr
    fs.exitCode = (ex.exitCode ?? 0) === actual.exitCode
    if (ex.returnCode && ex.returnCode !== '') fs.returnCode = ex.returnCode === actual.returnCode
    if (ex.result && ex.result !== '') fs.result = ex.result === actual.result
    if (ex.error && ex.error !== '') fs.error = ex.error === actual.error

    tc.fieldStatus = fs
    tc.status = Object.values(fs).filter(v => v === false).length === 0 ? 'pass' : 'fail'

    feather.value.destroy(interp)
  } catch (e) {
    tc.status = 'fail'
    tc.lastRun = tc.lastRun ?? {}
    tc.lastRun.error = (tc.lastRun.error || '') + '\n[Host error] ' + (e?.message ?? String(e))
  }
}

onMounted(() => {
  allTests.value = parseTestSuite(props.source)
})

watch(() => props.source, (newSource) => {
  allTests.value = parseTestSuite(newSource)
})
</script>

<style scoped>
.test-search {
  margin: 24px 0;
}

.search-box {
  display: flex;
  align-items: center;
  gap: 12px;
  margin-bottom: 16px;
}

.search-input {
  flex: 1;
  padding: 12px 16px;
  font-size: 16px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
  background: var(--vp-c-bg);
  color: var(--vp-c-text-1);
  outline: none;
}

.search-input:focus {
  border-color: var(--vp-c-brand-1);
}

.search-input::placeholder {
  color: var(--vp-c-text-3);
}

.result-count {
  font-size: 14px;
  color: var(--vp-c-text-2);
  white-space: nowrap;
}

.parse-error, .loading, .no-results {
  padding: 16px;
  background: var(--vp-c-bg-soft);
  border-radius: 8px;
  color: var(--vp-c-text-2);
}

.parse-error {
  color: #ef4444;
}

.results {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.test-case {
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
  overflow: hidden;
  background: var(--vp-c-bg-soft);
}

.test-case.pass { border-color: #22c55e40; }
.test-case.fail { border-color: #ef444440; }

.test-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 12px 16px;
  cursor: pointer;
  user-select: none;
}

.test-header:hover {
  background: var(--vp-c-bg-alt);
}

.test-title {
  display: flex;
  align-items: center;
  gap: 10px;
}

.status-icon {
  font-size: 14px;
  width: 20px;
  text-align: center;
}

.test-case.pass .status-icon { color: #22c55e; }
.test-case.fail .status-icon { color: #ef4444; }
.test-case.running .status-icon { color: var(--vp-c-brand-1); }

.name {
  font-weight: 500;
  color: var(--vp-c-text-1);
}

.name :deep(mark) {
  background: var(--vp-c-brand-1);
  color: white;
  padding: 0 2px;
  border-radius: 2px;
}

.test-actions {
  display: flex;
  gap: 8px;
}

.run-btn {
  padding: 4px 12px;
  border-radius: 4px;
  font-size: 12px;
  cursor: pointer;
  background: var(--vp-c-brand-1);
  color: white;
  border: none;
}

.run-btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.test-body {
  padding: 16px;
  border-top: 1px solid var(--vp-c-divider);
  background: var(--vp-c-bg);
}

.editor-section {
  margin-bottom: 16px;
}

.section-label {
  font-size: 12px;
  font-weight: 600;
  color: var(--vp-c-text-2);
  margin-bottom: 8px;
  text-transform: uppercase;
}

.results-section {
  margin-top: 16px;
}

.result-columns {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 16px;
}

@media (max-width: 640px) {
  .result-columns {
    grid-template-columns: 1fr;
  }
}

.result-column {
  background: var(--vp-c-bg-soft);
  border-radius: 6px;
  padding: 12px;
}

.column-header {
  font-size: 12px;
  font-weight: 600;
  color: var(--vp-c-text-2);
  margin-bottom: 8px;
  text-transform: uppercase;
}

.result-field {
  margin-bottom: 8px;
  padding: 8px;
  border-radius: 4px;
  background: var(--vp-c-bg);
}

.result-field.pass { background: #22c55e15; }
.result-field.fail { background: #ef444420; }

.field-label {
  font-size: 11px;
  font-weight: 600;
  color: var(--vp-c-text-3);
  text-transform: uppercase;
  display: block;
  margin-bottom: 4px;
}

.result-field pre {
  margin: 0;
  font-size: 13px;
  font-family: var(--vp-font-family-mono);
  white-space: pre-wrap;
  word-break: break-all;
  color: var(--vp-c-text-1);
}

.more-results {
  width: 100%;
  padding: 12px 16px;
  text-align: center;
  color: var(--vp-c-text-2);
  font-size: 14px;
  background: var(--vp-c-bg-soft);
  border-radius: 8px;
  border: 1px solid var(--vp-c-divider);
  cursor: pointer;
}

.more-results:hover {
  border-color: var(--vp-c-brand-1);
  color: var(--vp-c-brand-1);
}
</style>
