# Oracle: Reference TCL Implementation

The oracle is a test harness host that embeds the **real TCL interpreter** (via libtcl).

Its purpose is to provide ground truth for feather's behavior. When developing new features
or debugging discrepancies, we can run the same test cases through both:

- `bin/gcl` (or other feather hosts) - our implementation
- `bin/oracle` - the reference TCL implementation

This allows us to verify that feather behaves identically to standard TCL for the subset
of commands we support.

## Building

The oracle uses pkg-config to find the system's TCL installation:

```bash
mise run build:oracle
```

Requires TCL development headers and library to be installed:
- macOS: `brew install tcl-tk`
- Debian/Ubuntu: `apt install tcl-dev`
- Fedora: `dnf install tcl-devel`

## Usage

```bash
echo 'expr {1 + 2}' | bin/oracle
```

Or with the harness:

```bash
harness run --host bin/oracle testcases/
```

## Harness Protocol

The oracle implements the same harness protocol as gcl:

- Reads script from stdin
- Writes result to stdout
- When `FEATHER_IN_HARNESS=1`, writes structured output to fd 3:
  - `return: TCL_OK` or `return: TCL_ERROR`
  - `result: <value>` (if non-empty)
  - `error: <message>` (if error occurred)
