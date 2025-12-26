# Run Tests

Run Feather's test suite in your browser. The interpreter is compiled to WebAssembly, so all tests run locally without a server.

<script setup>
import TestRunner from '../../.vitepress/components/TestRunner.vue'

// Load all test case files
const testFiles = import.meta.glob('../../testcases/**/*.html', { query: '?raw', eager: true })
const allSources = Object.values(testFiles).map(m => m.default).join('\n')
</script>

<TestRunner :source="allSources" />
