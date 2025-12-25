# Feather Documentation Site

VitePress-based documentation and playground for the Feather programming language.

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

## Deployment

Deployed to Cloudflare Pages as `feather-lang` project.
