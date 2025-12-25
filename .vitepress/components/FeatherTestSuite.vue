<template>
  <div class="feather-suite">
    <div v-if="parseError" class="parse-error">Failed to parse tests: {{ parseError }}</div>
    <div v-else-if="!ready" class="loading">Loading Feather interpreter...</div>

    <div class="suite-header" v-if="tests.length">
      <span class="suite-stats">
        {{ passCount }}/{{ tests.length }} passing
      </span>
      <button class="run-all-btn" @click="runAllTests" :disabled="!ready || running">
        {{ running ? 'Running...' : '▶ Run All' }}
      </button>
    </div>

    <div
      v-for="(tc, i) in tests"
      :key="tc.name || i"
      class="test-case"
      :class="tc.status"
    >
      <div class="test-header" @click="toggleExpanded(i)">
        <div class="test-title">
          <span class="status-icon">
            <span v-if="tc.status === 'pass'">✓</span>
            <span v-else-if="tc.status === 'fail'">✗</span>
            <span v-else-if="tc.status === 'running'">⏳</span>
            <span v-else>○</span>
          </span>
          <span class="name">{{ tc.name }}</span>
        </div>
        <div class="test-actions" @click.stop>
          <button class="run-btn" @click="runTest(i)" :disabled="!ready">Run</button>
          <button class="reset-btn" @click="resetTest(i)">Reset</button>
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
              <div class="result-field" :class="fieldClass(tc.fieldStatus.stdout)" v-if="tc.expected.stdout !== undefined">
                <span class="field-label">stdout:</span>
                <pre>{{ tc.expected.stdout || '(empty)' }}</pre>
              </div>
              <div class="result-field" :class="fieldClass(tc.fieldStatus.error)" v-if="tc.expected.error !== undefined">
                <span class="field-label">error:</span>
                <pre>{{ tc.expected.error || '(empty)' }}</pre>
              </div>
              <div class="result-field" :class="fieldClass(tc.fieldStatus.returnCode)" v-if="tc.expected.returnCode">
                <span class="field-label">return:</span>
                <pre>{{ tc.expected.returnCode }}</pre>
              </div>
              <div class="result-field" :class="fieldClass(tc.fieldStatus.exitCode)" v-if="tc.expected.exitCode !== undefined">
                <span class="field-label">exit-code:</span>
                <pre>{{ tc.expected.exitCode }}</pre>
              </div>
              <div class="result-field" :class="fieldClass(tc.fieldStatus.result)" v-if="tc.expected.result">
                <span class="field-label">result:</span>
                <pre>{{ tc.expected.result }}</pre>
              </div>
            </div>
            <div class="result-column">
              <div class="column-header">Actual</div>
              <div class="result-field" :class="fieldClass(tc.fieldStatus.stdout)" v-if="tc.expected.stdout !== undefined">
                <span class="field-label">stdout:</span>
                <pre>{{ tc.lastRun.stdout || '(empty)' }}</pre>
              </div>
              <div class="result-field" :class="fieldClass(tc.fieldStatus.error)" v-if="tc.expected.error !== undefined">
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
              <div class="result-field" :class="fieldClass(tc.fieldStatus.result)" v-if="tc.expected.result">
                <span class="field-label">result:</span>
                <pre>{{ tc.lastRun.result }}</pre>
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
const tests = ref([])
const parseError = ref(null)
const running = ref(false)

const RETURN_CODE_NAMES = {
  [TCL_OK]: 'TCL_OK',
  [TCL_ERROR]: 'TCL_ERROR',
  [TCL_RETURN]: 'TCL_RETURN',
  [TCL_BREAK]: 'TCL_BREAK',
  [TCL_CONTINUE]: 'TCL_CONTINUE',
}

const passCount = computed(() => tests.value.filter(t => t.status === 'pass').length)

// Matches harness/parser.go normalizeLines: trim each line, remove leading/trailing empty lines
function normalizeLines(content) {
  if (!content) return ''
  const lines = content.split('\n')
  // Trim each line
  for (let i = 0; i < lines.length; i++) {
    lines[i] = lines[i].trim()
  }
  // Skip leading empty lines
  let start = 0
  while (start < lines.length && lines[start] === '') {
    start++
  }
  // Skip trailing empty lines
  let end = lines.length
  while (end > start && lines[end - 1] === '') {
    end--
  }
  if (start >= end) {
    return ''
  }
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
      // Check if element exists and get normalized content
      const getField = (tag) => {
        const child = el.querySelector(tag)
        if (!child) return { present: false, value: undefined }
        const raw = decodeHtmlEntities(child.textContent ?? '')
        return { present: true, value: normalizeLines(raw) }
      }

      const name = el.getAttribute('name') ?? ''
      const scriptField = getField('script')
      const script = scriptField.value ?? ''
      
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
  return {
    pass: status === true,
    fail: status === false,
  }
}

function toggleExpanded(idx) {
  tests.value[idx].expanded = !tests.value[idx].expanded
}

function resetTest(idx) {
  const tc = tests.value[idx]
  tc.script = tc.originalScript
  tc.lastRun = null
  tc.status = 'idle'
  tc.fieldStatus = {}
}

async function runTest(idx, autoExpand = true) {
  if (!ready.value) return

  const tc = tests.value[idx]
  tc.status = 'running'
  tc.lastRun = null
  tc.fieldStatus = {}
  if (autoExpand) tc.expanded = true

  const stdout = []
  const stderr = []

  await new Promise(resolve => setTimeout(resolve, 0))

  try {
    const interp = createTestInterpreter(feather.value, {
      onStdout: line => stdout.push(line),
      onStderr: line => stderr.push(line),
    })

    let returnCode = 'TCL_OK'
    let resultStr = ''
    let errorStr = ''
    let exitCode = 0

    // Check for parse errors first
    const parseResult = feather.value.parse(interp, tc.script)
    
    if (parseResult.status === 1) { // TCL_PARSE_INCOMPLETE
      returnCode = 'TCL_OK'
      resultStr = parseResult.result
      exitCode = 2
    } else if (parseResult.status === 2) { // TCL_PARSE_ERROR
      returnCode = 'TCL_ERROR'
      resultStr = parseResult.result
      errorStr = parseResult.errorMessage || ''
      exitCode = 3
    } else {
      // Parse succeeded, now evaluate
      try {
        const evalResult = feather.value.eval(interp, tc.script)
        resultStr = evalResult || ''
        // Like tester.js: print the final result to stdout if non-empty
        if (evalResult !== '') {
          stdout.push(evalResult)
        }
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

    const actual = {
      returnCode,
      result: resultStr,
      error: normalizeLines(errorStr),
      stdout: normalizeLines(stdout.join('\n')),
      stderr: normalizeLines(stderr.join('\n')),
      exitCode,
    }

    tc.lastRun = actual

    // Compare with expected (matching harness/runner.go logic)
    const fs = {}
    const ex = tc.expected

    // Only compare stdout if <stdout> was present in the test file
    if (ex.stdoutSet) {
      fs.stdout = (ex.stdout ?? '') === actual.stdout
    }

    // stderr is always compared (empty string if not specified)
    fs.stderr = (ex.stderr ?? '') === actual.stderr

    // exit code is always compared (default 0 if not specified)
    const expectedExitCode = ex.exitCode ?? 0
    fs.exitCode = expectedExitCode === actual.exitCode

    // return code only compared if specified and non-empty
    if (ex.returnCode && ex.returnCode !== '') {
      fs.returnCode = ex.returnCode === actual.returnCode
    }

    // result only compared if specified and non-empty
    if (ex.result && ex.result !== '') {
      fs.result = ex.result === actual.result
    }

    // error only compared if specified and non-empty
    if (ex.error && ex.error !== '') {
      fs.error = ex.error === actual.error
    }

    tc.fieldStatus = fs

    const mismatches = Object.values(fs).filter(v => v === false)
    tc.status = mismatches.length === 0 ? 'pass' : 'fail'

    feather.value.destroy(interp)
  } catch (e) {
    tc.status = 'fail'
    tc.lastRun = tc.lastRun ?? {}
    tc.lastRun.error = (tc.lastRun.error || '') + '\n[Host error] ' + (e?.message ?? String(e))
  }
}

async function runAllTests() {
  running.value = true
  for (let i = 0; i < tests.value.length; i++) {
    await runTest(i, false)
  }
  running.value = false
}

onMounted(() => {
  tests.value = parseTestSuite(props.source)
})

watch(() => props.source, (newSource) => {
  tests.value = parseTestSuite(newSource)
})
</script>

<style scoped>
.feather-suite {
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

.suite-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 16px;
  padding: 12px 16px;
  background: var(--vp-c-bg-soft);
  border-radius: 8px;
}

.suite-stats {
  font-weight: 600;
  color: var(--vp-c-text-1);
}

.run-all-btn {
  background: var(--vp-c-brand-1);
  color: white;
  border: none;
  padding: 8px 16px;
  border-radius: 6px;
  font-weight: 600;
  cursor: pointer;
}

.run-all-btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.test-case {
  margin-bottom: 8px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
  overflow: hidden;
  background: var(--vp-c-bg-soft);
}

.test-case.pass {
  border-color: #22c55e40;
}

.test-case.fail {
  border-color: #ef444440;
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
}

.test-case.pass .status-icon { color: #22c55e; }
.test-case.fail .status-icon { color: #ef4444; }
.test-case.running .status-icon { color: var(--vp-c-brand-1); }

.name {
  font-weight: 500;
  color: var(--vp-c-text-1);
}

.test-actions {
  display: flex;
  gap: 8px;
}

.run-btn, .reset-btn {
  padding: 4px 12px;
  border-radius: 4px;
  font-size: 12px;
  cursor: pointer;
  border: 1px solid var(--vp-c-divider);
  background: var(--vp-c-bg);
  color: var(--vp-c-text-1);
}

.run-btn {
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

.result-field.pass {
  background: #22c55e15;
}

.result-field.fail {
  background: #ef444420;
}

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
