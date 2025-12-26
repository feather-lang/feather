# Test harness

HTML files in this directory are actually test definitions,
interpreted and executed by the harness command.

No HTML entities are needed in script tags - we are using a compliant html parser for this.

## Test case structure

```html
<test-case name="descriptive name">
  <script>tcl code here</script>
  <return>TCL_OK</return>
  <error></error>
  <stdout>expected output</stdout>
  <stderr></stderr>
  <exit-code>0</exit-code>
</test-case>
```

## Important rules

### Verify expectations against the oracle

Before writing or fixing a test, verify the expected behavior against `bin/oracle`:

```bash
echo 'your tcl code' | bin/oracle
```

Build the oracle with `mise build:oracle` if needed.

### Error test cases

For tests expecting `TCL_ERROR`:
- Set `<return>TCL_ERROR</return>`
- Set `<error>expected error message</error>`
- Set `<exit-code>1</exit-code>`
- **Do NOT include `<stdout>` tag** - omit it entirely

The error message goes to both stdout and the harness error channel.
Including `<stdout></stdout>` (empty) will cause a mismatch because
the error message appears in stdout.

```html
<!-- CORRECT: no <stdout> tag for error cases -->
<test-case name="format with non-integer for %d">
  <script>format "%d" hello</script>
  <return>TCL_ERROR</return>
  <error>expected integer but got "hello"</error>
  <stderr></stderr>
  <exit-code>1</exit-code>
</test-case>

<!-- WRONG: <stdout></stdout> will fail -->
<test-case name="format with non-integer for %d">
  <script>format "%d" hello</script>
  <return>TCL_ERROR</return>
  <error>expected integer but got "hello"</error>
  <stdout></stdout>  <!-- This causes mismatch! -->
  <stderr></stderr>
  <exit-code>1</exit-code>
</test-case>
```

### Float vs integer results

TCL has specific rules about numeric return types:
- `round()` always returns an integer
- `abs()` returns int for int input, double for double input
- `exp()`, `sin()`, `cos()`, etc. always return doubles (e.g., `exp(0)` → `1.0`)

Verify with the oracle when uncertain.
