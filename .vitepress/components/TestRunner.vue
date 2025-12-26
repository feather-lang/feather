<template>
  <div class="test-runner">
    <div v-if="parseError" class="parse-error">Failed to parse tests: {{ parseError }}</div>
    <div v-else-if="!ready" class="loading">Loading Feather interpreter...</div>

    <div class="controls" v-if="ready">
      <button class="run-btn" @click="runAllTests" :disabled="running">
        {{ running ? 'Running...' : '▶ Run All Tests' }}
      </button>
      <span class="test-count">{{ totalTests }} tests</span>
    </div>

    <div class="progress" v-if="running || completed > 0">
      <div class="progress-bar">
        <div class="progress-fill pass" :style="{ width: passPercent + '%' }"></div>
        <div class="progress-fill fail" :style="{ width: failPercent + '%' }"></div>
      </div>
      <div class="progress-stats">
        <span class="completed">{{ completed }}/{{ totalTests }}</span>
        <span class="passed" v-if="passed > 0">✓ {{ passed }}</span>
        <span class="failed" v-if="failed > 0">✗ {{ failed }}</span>
      </div>
    </div>

    <div class="summary" v-if="!running && completed > 0">
      <span :class="failed === 0 ? 'all-pass' : 'has-failures'">
        {{ failed === 0 ? 'All tests passed!' : `${failed} test${failed === 1 ? '' : 's'} failed` }}
      </span>
    </div>

    <div class="failures" v-if="failedTests.length > 0">
      <h3>Failed Tests</h3>
      <div
        v-for="tc in failedTests"
        :key="tc.name"
        class="test-case fail"
      >
        <div class="test-header" @click="tc.expanded = !tc.expanded">
          <div class="test-title">
            <span class="status-icon">✗</span>
            <span class="name">{{ tc.name }}</span>
          </div>
        </div>

        <div class="test-body" v-show="tc.expanded">
          <div class="editor-section">
            <div class="section-label">Script</div>
            <CodeEditor v-model="tc.script" language="tcl" :minHeight="60" />
          </div>

          <div class="results-section">
            <div class="result-columns">
              <div class="result-column">
                <div class="column-header">Expected</div>
                <div class="result-field" :class="fieldClass(tc.fieldStatus.stdout)" v-if="tc.expected.stdoutSet">
                  <span class="field-label">stdout:</span>
                  <pre>{{ tc.expected.stdout || '(empty)' }}</pre>
                </div>
                <div class="result-field" :class="fieldClass(tc.fieldStatus.stderr)" v-if="tc.expected.stderr !== undefined">
                  <span class="field-label">stderr:</span>
                  <pre>{{ tc.expected.stderr || '(empty)' }}</pre>
                </div>
                <div class="result-field" :class="fieldClass(tc.fieldStatus.error)" v-if="tc.expected.error">
                  <span class="field-label">error:</span>
                  <pre>{{ tc.expected.error }}</pre>
                </div>
                <div class="result-field" :class="fieldClass(tc.fieldStatus.returnCode)" v-if="tc.expected.returnCode">
                  <span class="field-label">return:</span>
                  <pre>{{ tc.expected.returnCode }}</pre>
                </div>
                <div class="result-field" :class="fieldClass(tc.fieldStatus.exitCode)" v-if="tc.expected.exitCode !== undefined">
                  <span class="field-label">exit-code:</span>
                  <pre>{{ tc.expected.exitCode }}</pre>
                </div>
              </div>
              <div class="result-column">
                <div class="column-header">Actual</div>
                <div class="result-field" :class="fieldClass(tc.fieldStatus.stdout)" v-if="tc.expected.stdoutSet">
                  <span class="field-label">stdout:</span>
                  <pre>{{ tc.lastRun.stdout || '(empty)' }}</pre>
                </div>
                <div class="result-field" :class="fieldClass(tc.fieldStatus.stderr)" v-if="tc.expected.stderr !== undefined">
                  <span class="field-label">stderr:</span>
                  <pre>{{ tc.lastRun.stderr || '(empty)' }}</pre>
                </div>
                <div class="result-field" :class="fieldClass(tc.fieldStatus.error)" v-if="tc.expected.error">
                  <span class="field-label">error:</span>
                  <pre>{{ tc.lastRun.error || '(empty)' }}</pre>
                </div>
                <div class="result-field" :class="fieldClass(tc.fieldStatus.returnCode)" v-if="tc.expected.returnCode">
                  <span class="field-label">return:</span>
                  <pre>{{ tc.lastRun.returnCode }}</pre>
                </div>
                <div class="result-field" :class="fieldClass(tc.fieldStatus.exitCode)" v-if="tc.expected.exitCode !== undefined">
                  <span class="field-label">exit-code:</span>
                  <pre>{{ tc.lastRun.exitCode }}</pre>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, watch } from 'vue'
import CodeEditor from './CodeEditor.vue'
import { useFeather, createTestInterpreter } from '../composables/useFeather.js'
import { TCL_OK, TCL_ERROR, TCL_RETURN, TCL_BREAK, TCL_CONTINUE } from '../feather.js'

const props = defineProps({
  source: { type: String, required: true }
})

const { feather, ready, error: wasmError } = useFeather()
const parseError = ref(null)
const running = ref(false)
const completed = ref(0)
const passed = ref(0)
const failed = ref(0)
const failedTests = ref([])
const totalTests = ref(0)

// Store parsed tests but don't render them
let allTests = []

const RETURN_CODE_NAMES = {
  [TCL_OK]: 'TCL_OK',
  [TCL_ERROR]: 'TCL_ERROR',
  [TCL_RETURN]: 'TCL_RETURN',
  [TCL_BREAK]: 'TCL_BREAK',
  [TCL_CONTINUE]: 'TCL_CONTINUE',
}

const passPercent = computed(() => {
  if (totalTests.value === 0) return 0
  return (passed.value / totalTests.value) * 100
})

const failPercent = computed(() => {
  if (totalTests.value === 0) return 0
  return (failed.value / totalTests.value) * 100
})

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

function runSingleTest(tc) {
  const stdout = []
  const stderr = []

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
      if (e.code === TCL_ERROR) {
        returnCode = 'TCL_ERROR'
        exitCode = 1
      } else {
        returnCode = RETURN_CODE_NAMES[e.code] || 'TCL_ERROR'
        exitCode = e.code || 1
      }
    }
  }

  feather.value.destroy(interp)

  const actual = {
    returnCode,
    result: resultStr,
    error: normalizeLines(errorStr),
    stdout: normalizeLines(stdout.join('\n')),
    stderr: normalizeLines(stderr.join('\n')),
    exitCode,
  }

  // Compare
  const fs = {}
  const ex = tc.expected

  if (ex.stdoutSet) fs.stdout = (ex.stdout ?? '') === actual.stdout
  fs.stderr = (ex.stderr ?? '') === actual.stderr
  fs.exitCode = (ex.exitCode ?? 0) === actual.exitCode
  if (ex.returnCode && ex.returnCode !== '') fs.returnCode = ex.returnCode === actual.returnCode
  if (ex.result && ex.result !== '') fs.result = ex.result === actual.result
  if (ex.error && ex.error !== '') fs.error = ex.error === actual.error

  const didPass = Object.values(fs).filter(v => v === false).length === 0

  return { didPass, actual, fieldStatus: fs }
}

async function runAllTests() {
  if (!ready.value) return

  running.value = true
  completed.value = 0
  passed.value = 0
  failed.value = 0
  failedTests.value = []

  // Process in batches to keep UI responsive
  const batchSize = 50
  for (let i = 0; i < allTests.length; i += batchSize) {
    const batch = allTests.slice(i, i + batchSize)
    
    for (const tc of batch) {
      try {
        const { didPass, actual, fieldStatus } = runSingleTest(tc)
        
        if (didPass) {
          passed.value++
        } else {
          failed.value++
          failedTests.value.push({
            ...tc,
            lastRun: actual,
            fieldStatus,
            expanded: false
          })
        }
      } catch (e) {
        failed.value++
        failedTests.value.push({
          ...tc,
          lastRun: { error: e?.message ?? String(e) },
          fieldStatus: {},
          expanded: false
        })
      }
      completed.value++
    }
    
    // Yield to UI
    await new Promise(resolve => setTimeout(resolve, 0))
  }

  running.value = false
}

onMounted(() => {
  allTests = parseTestSuite(props.source)
  totalTests.value = allTests.length
})

watch(() => props.source, (newSource) => {
  allTests = parseTestSuite(newSource)
  totalTests.value = allTests.length
})
</script>

<style scoped>
.test-runner {
  margin: 24px 0;
}

.parse-error, .loading {
  padding: 16px;
  background: var(--vp-c-bg-soft);
  border-radius: 8px;
  color: var(--vp-c-text-2);
}

.parse-error {
  color: #ef4444;
}

.controls {
  display: flex;
  align-items: center;
  gap: 16px;
  margin-bottom: 16px;
}

.run-btn {
  background: var(--vp-c-brand-1);
  color: white;
  border: none;
  padding: 12px 24px;
  border-radius: 8px;
  font-weight: 600;
  font-size: 16px;
  cursor: pointer;
}

.run-btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.test-count {
  color: var(--vp-c-text-2);
}

.progress {
  margin-bottom: 16px;
}

.progress-bar {
  height: 8px;
  background: var(--vp-c-bg-soft);
  border-radius: 4px;
  overflow: hidden;
  margin-bottom: 8px;
  display: flex;
}

.progress-fill {
  height: 100%;
  transition: width 0.1s ease;
}

.progress-fill.pass {
  background: #22c55e;
}

.progress-fill.fail {
  background: #ef4444;
}

.progress-stats {
  display: flex;
  gap: 16px;
  font-size: 14px;
}

.completed { color: var(--vp-c-text-2); }
.passed { color: #22c55e; }
.failed { color: #ef4444; }

.summary {
  padding: 16px;
  background: var(--vp-c-bg-soft);
  border-radius: 8px;
  margin-bottom: 16px;
  font-weight: 600;
}

.all-pass { color: #22c55e; }
.has-failures { color: #ef4444; }

.failures h3 {
  margin: 24px 0 16px;
  color: var(--vp-c-text-1);
}

.test-case {
  margin-bottom: 8px;
  border: 1px solid #ef444440;
  border-radius: 8px;
  overflow: hidden;
  background: var(--vp-c-bg-soft);
}

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
  color: #ef4444;
}

.name {
  font-weight: 500;
  color: var(--vp-c-text-1);
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
</style>
