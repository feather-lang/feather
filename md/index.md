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

Feather is designed to be embedded into host language.

The core implementation provides the language semantics, but all memory allocations,
I/O, Unicode and floating point operations are provided by the host.

Chances are that if you are embedding a scripting language in 2025, your host language
already _has_ an implementation of everything and duplicating Unicode tables or
getting two event loops to play nice with each other is more trouble than it's worth.

<div class="runs-everywhere-cards">
  <div class="platform-card">
    <div class="platform-logo-col">
      <img src="/go-logo.svg" alt="Go Logo" class="platform-logo" />
    </div>
    <div class="platform-text">
      <h3 data-status="alpha">Go</h3>
      <p>The reference host implementation, with an easy embedding API.</p>
	  <p>Quickly expose functions and structs from your program for manipulation through Feather.</p>
    </div>
  </div>
  <div class="platform-card">
    <div class="platform-logo-col">
      <img src="/webassembly-logo.svg" alt="WebAssembly Logo" class="platform-logo" />
    </div>
    <div class="platform-text">
      <h3 data-status="alpha">JavaScript / WASM</h3>
      <p>Run in browsers and Node.js via WebAssembly. Works anywhere WASM runs.</p>
	  <p>Host bindings for the node.js and the browser are provided.</p>
    </div>
  </div>
  <div class="platform-card">
    <div class="platform-logo-col">
      <img src="/swift-logo.svg" alt="Swift Logo" class="platform-logo" />
    </div>
    <div class="platform-text">
      <h3 data-status="planned">Swift</h3>
      <p>Host bindings for Swift, to allow end users of apps to change their behavior at runtime.</p>
      <p>Make any app as configurable as neovim and Emacs.</p>
    </div>
  </div>
  <div class="platform-card">
    <div class="platform-logo-col">
      <img src="/java-logo.svg" alt="Java Logo" class="platform-logo" />
    </div>
    <div class="platform-text">
      <h3 data-status="planned">Java</h3>
      <p>Java is pretty dynamic already, but not jshell is clunky and other languages bring their own implementation of everything.</p>
      <p>Target use cases: runtime configuration of webserver, user-scriptable cross-platform GUI apps.</p>
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
  flex: 1 1 calc(33% - 24px);
  max-width: calc(33% - 12px);
  min-width: 280px;
  padding: 24px;
  border-radius: 12px;
  background: var(--vp-c-bg-soft);
  border: 1px solid var(--vp-c-divider);
  display: flex;
  gap: 20px;
  align-items: center;
}

@media (max-width: 1024px) {
  .platform-card {
    flex: 1 1 calc(50% - 24px);
    max-width: calc(50% - 12px);
  }
}

@media (max-width: 640px) {
  .platform-card {
    flex: 1 1 100%;
    max-width: 100%;
  }
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
  display: flex;
  align-items: center;
  gap: 8px;
}

.platform-text h3::after {
  content: attr(data-status);
  font-size: 0.65em;
  font-weight: 600;
  text-transform: uppercase;
  padding: 2px 6px;
  border-radius: 4px;
  background: var(--vp-c-gray-2);
  color: var(--vp-c-text-2);
}

.platform-text h3[data-status="alpha"]::after {
  background: var(--vp-c-green-soft);
  color: var(--vp-c-green-1);
}

.platform-text h3[data-status="planned"]::after {
  background: var(--vp-c-yellow-soft);
  color: var(--vp-c-yellow-1);
}

.platform-text p {
  margin: 0;
  color: var(--vp-c-text-2);
  font-size: 0.95em;
}
</style>

