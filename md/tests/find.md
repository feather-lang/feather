# Find Tests

Search the test suite for examples of specific commands or behaviors. Expand any result to see the script and run it.

<script setup>
import { ref } from 'vue'
import TestCaseSearch from '../../.vitepress/components/TestCaseSearch.vue'

// Load all test case files
const testFiles = import.meta.glob('../../testcases/**/*.html', { query: '?raw', eager: true })
const allSources = Object.values(testFiles).map(m => m.default).join('\n')

const searchRef = ref(null)
const examples = ['proc', 'uplevel', 'namespace eval']

function search(q) {
  searchRef.value?.setQuery(q)
}
</script>

<div class="example-searches">
  <span class="label">Try</span>
  <button v-for="ex in examples" :key="ex" @click="search(ex)" class="example-btn">{{ ex }}</button>
</div>

<style>
.example-searches {
  display: flex;
  align-items: center;
  gap: 8px;
  flex-wrap: wrap;
  margin-bottom: 16px;
}
.example-searches .label {
  color: var(--vp-c-text-2);
  font-weight: 500;
}
.example-btn {
  padding: 4px 12px;
  border-radius: 6px;
  border: 1px solid var(--vp-c-divider);
  background: var(--vp-c-bg-soft);
  color: var(--vp-c-text-1);
  font-size: 14px;
  cursor: pointer;
}
.example-btn:hover {
  border-color: var(--vp-c-brand-1);
  color: var(--vp-c-brand-1);
}
</style>

<TestCaseSearch ref="searchRef" :source="allSources" />
