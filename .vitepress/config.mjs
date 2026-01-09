import { defineConfig } from "vitepress";

// https://vitepress.dev/reference/site-config
export default defineConfig({
  srcDir: "md",
  head: [["link", { rel: "icon", href: "/feather-logo.png" }]],
  ignoreDeadLinks: [/\.wasm$/, /\.js$/],

  title: "Feather Programming Language",
  description:
    "Feather is an embeddable TCL interpreter for adding interactive command shells to any application.",
  themeConfig: {
    logo: "/feather-logo.png",
    // https://vitepress.dev/reference/default-theme-config
    nav: [
      { text: "Home", link: "/" },
      { text: "Philosophy", link: "/philosophy" },
      { text: "Documentation", link: "/introduction" },
    ],

    sidebar: [
      {
        text: "Getting Started",
        items: [
          { text: "Introduction", link: "/introduction" },
          { text: "In 5 Minutes", link: "/in-5-minutes" },
          { text: "Go", link: "/go" },
          { text: "Example: Turtle Graphics", link: "/go-turtle" },
          { text: "WASM", link: "/wasm" },
        ],
      },
      {
        text: "Feather Extensions",
        items: [
          { text: "Command Completion", link: "/completion" },
        ],
      },
      {
        text: "Built-in Commands",
        collapsed: false,
        items: [
          { text: "Overview", link: "/commands/" },
          { text: "append", link: "/commands/append" },
          { text: "apply", link: "/commands/apply" },
          { text: "break", link: "/commands/break" },
          { text: "catch", link: "/commands/catch" },
          { text: "concat", link: "/commands/concat" },
          { text: "continue", link: "/commands/continue" },
          { text: "dict", link: "/commands/dict" },
          { text: "error", link: "/commands/error" },
          { text: "eval", link: "/commands/eval" },
          { text: "expr", link: "/commands/expr" },
          { text: "for", link: "/commands/for" },
          { text: "foreach", link: "/commands/foreach" },
          { text: "format", link: "/commands/format" },
          { text: "global", link: "/commands/global" },
          { text: "if", link: "/commands/if" },
          { text: "incr", link: "/commands/incr" },
          { text: "info", link: "/commands/info" },
          { text: "join", link: "/commands/join" },
          { text: "lappend", link: "/commands/lappend" },
          { text: "lassign", link: "/commands/lassign" },
          { text: "lindex", link: "/commands/lindex" },
          { text: "linsert", link: "/commands/linsert" },
          { text: "list", link: "/commands/list" },
          { text: "llength", link: "/commands/llength" },
          { text: "lmap", link: "/commands/lmap" },
          { text: "lrange", link: "/commands/lrange" },
          { text: "lrepeat", link: "/commands/lrepeat" },
          { text: "lreplace", link: "/commands/lreplace" },
          { text: "lreverse", link: "/commands/lreverse" },
          { text: "lsearch", link: "/commands/lsearch" },
          { text: "lset", link: "/commands/lset" },
          { text: "lsort", link: "/commands/lsort" },
          { text: "namespace", link: "/commands/namespace" },
          { text: "proc", link: "/commands/proc" },
          { text: "rename", link: "/commands/rename" },
          { text: "return", link: "/commands/return" },
          { text: "scan", link: "/commands/scan" },
          { text: "set", link: "/commands/set" },
          { text: "split", link: "/commands/split" },
          { text: "string", link: "/commands/string" },
          { text: "subst", link: "/commands/subst" },
          { text: "switch", link: "/commands/switch" },
          { text: "tailcall", link: "/commands/tailcall" },
          { text: "throw", link: "/commands/throw" },
          { text: "trace", link: "/commands/trace" },
          { text: "try", link: "/commands/try" },
          { text: "unset", link: "/commands/unset" },
          { text: "uplevel", link: "/commands/uplevel" },
          { text: "upvar", link: "/commands/upvar" },
          { text: "usage", link: "/commands/usage" },
          { text: "variable", link: "/commands/variable" },
          { text: "while", link: "/commands/while" },
        ],
      },
      {
        text: "Testing",
        items: [
          { text: "Run Tests", link: "/tests/run" },
          { text: "Find Tests", link: "/tests/find" },
        ],
      },
      {
        text: "About",
        items: [
          { text: "Philosophy", link: "/philosophy" },
          { text: "Credits", link: "/credits" },
        ],
      },
    ],

    socialLinks: [
      { icon: "github", link: "https://github.com/feather-lang/feather" },
    ],
  },
  vite: {
    server: {
      allowedHosts: ['golf-lima.exe.xyz']
    },
    ssr: {
      noExternal: ['feather.js']
    },
    build: {
      rollupOptions: {
        onwarn(warning, warn) {
          if (warning.message.includes('Module "fs" has been externalized')) return
          warn(warning)
        }
      }
    }
  }
});
