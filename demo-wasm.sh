#!/usr/bin/env bash
# Demonstration of feather-tester running as WASM through Node.js

set -e
cd "$(dirname "$0")"

echo "=== Feather WASM Demonstration ==="
echo ""
echo "Running feather interpreter compiled to WebAssembly via Node.js"
echo "The C interpreter is compiled to WASM using Zig,"
echo "and tester.js provides the host environment in JavaScript."
echo ""

echo "--- Test 1: Basic commands ---"
echo 'say-hello' | node --experimental-wasm-type-reflection js/tester.js
echo ""

echo "--- Test 2: Echo with arguments ---"
echo 'echo Hello World from WASM!' | node --experimental-wasm-type-reflection js/tester.js
echo ""

echo "--- Test 3: Variables and substitution ---"
cat <<'EOF' | node --experimental-wasm-type-reflection js/tester.js
set x 42
set y 100
echo The values are $x and $y
EOF
echo ""

echo "--- Test 4: Counter foreign object ---"
cat <<'EOF' | node --experimental-wasm-type-reflection js/tester.js
set c [Counter new]
echo Created counter: $c
$c set 10
echo Initial value: [$c get]
$c incr
$c incr
echo After 2 increments: [$c get]
$c add 5
echo After adding 5: [$c get]
$c destroy
EOF
echo ""

echo "--- Test 5: Running test harness ---"
echo "Running eval.html test suite..."
bin/harness run --host js/tester.js testcases/eval.html
echo ""

echo "Running foreign object test suite..."
bin/harness run --host js/tester.js testcases/feather/foreign.html
echo ""

echo "=== All demonstrations completed successfully ==="
