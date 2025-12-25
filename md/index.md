---
# https://vitepress.dev/reference/default-theme-home-page
layout: home

hero:
  name: "Feather"
  tagline: "Effortlessly add a command shell to your application: for your users, and your agents."
  image:
    src: /feather-logo.png
    alt: Feather Logo
  actions:
    - theme: brand
      text: Try It Live
      link: "#playground"
    - theme: alt
      text: GitHub
      link: https://github.com/feather-lang/feather

features:
  - title: Embeddable
    details: Designed from the ground up to be embedded into host applications. No I/O, no event loop - your host stays in control.
  - title: Inspectable
    details: Every aspect of a running Feather program can be inspected and modified from within, perfect for debugging and agentic coding.
  - title: TCL-Compatible
    details: A faithful implementation of the TCL core language with control flow, data structures, metaprogramming, and string manipulation.
---

<script setup>
import FeatherPlayground from '../.vitepress/components/FeatherPlayground.vue'
</script>

<div id="playground">
  <FeatherPlayground />
</div>

## Why Feather?

Feather fills a niche where you have an existing application and want to add an interactive console or networked REPL:

- **Give agents access to your program** - like Chrome DevTools, but for YOUR application
- **Configure servers at runtime** - hot-reload configuration without restarts
- **Quake-style consoles** - add developer consoles to games and applications
- **Configuration file format** - use a real programming language for config
- **User customization** - let users script and extend your software

## Feather vs Lua

Lua excels at programming extensions for large applications.

Feather is for **short, interactive programs** - similar to bash. It's the thin glue layer that lets you talk to your programs while they're running.

## Runs Everywhere

Feather compiles to **WebAssembly**, running in browsers and any WASM runtime. The Go implementation serves as the reference host.

<div class="runs-everywhere-cards">
  <div class="platform-card">
    <div class="platform-logo-col">
      <img src="/go-logo.svg" alt="Go Logo" class="platform-logo" />
    </div>
    <div class="platform-text">
      <h3>Go</h3>
      <p>Reference implementation. Embed Feather in any Go application with a simple API.</p>
    </div>
  </div>
  <div class="platform-card">
    <div class="platform-logo-col">
      <img src="/webassembly-logo.svg" alt="WebAssembly Logo" class="platform-logo" />
    </div>
    <div class="platform-text">
      <h3>JavaScript / WASM</h3>
      <p>Run in browsers and Node.js via WebAssembly. Works anywhere WASM runs.</p>
    </div>
  </div>
</div>

<style>
.runs-everywhere-cards {
  display: flex;
  gap: 24px;
  margin-top: 24px;
  flex-wrap: wrap;
}

.platform-card {
  flex: 1;
  min-width: 320px;
  padding: 24px;
  border-radius: 12px;
  background: var(--vp-c-bg-soft);
  border: 1px solid var(--vp-c-divider);
  display: flex;
  gap: 20px;
  align-items: center;
}

.platform-logo-col {
  flex-shrink: 0;
  width: 80px;
  display: flex;
  justify-content: center;
}

.platform-logo {
  height: 56px;
  width: auto;
}

.platform-text {
  text-align: left;
}

.platform-text h3 {
  margin: 0 0 8px 0;
  font-size: 1.2em;
}

.platform-text p {
  margin: 0;
  color: var(--vp-c-text-2);
  font-size: 0.95em;
}
</style>

