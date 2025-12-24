# Go Host Implementation

This is the meat and bones of the implementation of the Go host.

./cmd/gcl/main.go is the thinnest possible wrapper to provider access
to the host implementation here.

We are targeting go 1.24

## Shimmering Rules

TCL objects can have multiple representations (string, int, list, etc.).
"Shimmering" is the lazy conversion between these representations.

**All shimmering logic MUST be centralized in `*Interp` methods:**

| Method | Conversion |
|--------|------------|
| `GetString(h FeatherObj)` | int/list → string |
| `GetInt(h FeatherObj)` | string → int |
| `GetList(h FeatherObj)` | string → list |

**Rules:**

1. Never access `Object` fields directly in callbacks - use the `Get*` methods
2. Each `Get*` method is responsible for:
   - Checking if the representation already exists
   - Converting from another representation if needed
   - Caching the result in the `Object` struct
3. New representations (float, dict) require:
   - New fields in `Object` struct
   - New `Get*` method on `*Interp`
   - Update `GetString()` if string representation is needed
4. C callbacks (`go*.go` exports) must delegate to these methods
