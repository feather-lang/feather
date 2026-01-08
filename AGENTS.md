# Feather Documentation Site

VitePress-based documentation and playground for the Feather programming language.

## Repository Structure

This repository uses **two orphan branches** with separate histories:

| Branch | Purpose |
|--------|---------|
| `main` | Feather interpreter source code (Go, C, WASM builds) |
| `page` | This VitePress documentation site (no shared commits with `main`) |

**Implications:**
- The `page` branch only contains docs/website files, not interpreter source
- `feather.wasm` and `feather.js` are **copies** extracted from the `main` branch build output
- To update WASM/JS files, use `mise run update-tests` which extracts from `../feather` (a separate checkout of `main`)
- Deploys go to Cloudflare Pages from the `page` branch

## Commands

Use `mise` to run all tasks (never use npm directly):

```bash
mise run dev      # Start development server
mise run build    # Build for production
mise run preview  # Preview production build
mise run deploy   # Deploy to Cloudflare Pages
```

Or use `bun` directly:

```bash
bun run dev
bun run build
bun install <package>  # Install dependencies (not npm install)
```

## Project Structure

```
.vitepress/
  config.mjs           # VitePress configuration (title, nav, sidebar, theme)
  dist/                # Build output
  theme/
    index.js           # Theme entry point (extends default theme)
    custom.css         # Custom brand colors based on Feather logo
  components/
    FeatherPlayground.vue  # Interactive WASM playground
    CodeEditor.vue         # CodeMirror 6 editor with Gruvbox themes
    tcl-lang.js            # TCL syntax highlighting for CodeMirror
  feather.js           # Feather WASM JavaScript bindings

md/                    # Source directory (srcDir in config)
  index.md             # Home page with hero, features, and playground
  public/
    feather-logo.png   # Logo (served at /feather-logo.png)
    feather.wasm       # Feather interpreter compiled to WASM
```

## Code Editor

The playground uses CodeMirror 6 with:
- **Gruvbox Dark/Light themes** - auto-switches with VitePress theme
- **Custom TCL language support** - in `tcl-lang.js`
- **Custom comment colors** - white on dark, black on light

### TCL Syntax Highlighting

TCL comments (`#`) are only recognized at command start:
- After `^\s*` (start of line)
- After `;\s*` (semicolon)
- After `[` (command substitution)

## Feather WASM Integration

Files copied from `~/projects/feather/js/`:
- `feather.js` → `.vitepress/feather.js` (ES module, imported by Vue)
- `feather.wasm` → `md/public/feather.wasm` (served statically)

The playground registers host commands:
- `puts` - output to the playground console
- `clock` - time functions
- `canvas` - drawing commands (clear, fill, stroke, rect, circle, line, polygon, text, font)

## Code Samples

All code samples in documentation must use the `<FeatherPlayground />` component so they are interactive and runnable. Never use static code blocks for Feather/TCL examples.

```vue
<script setup>
import FeatherPlayground from '../.vitepress/components/FeatherPlayground.vue'
</script>

<FeatherPlayground />
```

## Deployment

Deployed to Cloudflare Pages as `feather-lang` project.
