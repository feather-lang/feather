# Find Tests

Search the test suite for examples of specific commands or behaviors. Expand any result to see the script and run it.

<script setup>
import TestCaseSearch from '../../.vitepress/components/TestCaseSearch.vue'

// Load all test case files
const testFiles = import.meta.glob('../../testcases/**/*.html', { query: '?raw', eager: true })
const allSources = Object.values(testFiles).map(m => m.default).join('\n')
</script>

<TestCaseSearch :source="allSources" />
