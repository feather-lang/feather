---
title: Test Suite
---

# Test Suite

<script setup>
import testSuiteXml from '../testcases/simple-command-invocation.html?raw'
import FeatherTestSuite from '../.vitepress/components/FeatherTestSuite.vue'
</script>

Interactive test runner for the Feather interpreter. Each test case shows a script, expected output, and can be run individually or all at once.

<ClientOnly>
  <FeatherTestSuite :source="testSuiteXml" />
</ClientOnly>
