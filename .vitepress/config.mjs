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
          { text: "WASM", link: "/wasm" },
        ],
      },
      {
        text: "Built-in Commands",
        collapsed: true,
        items: [
          { text: "Overview", link: "/commands/" },
          {
            text: "Control Flow",
            collapsed: true,
            items: [
              { text: "if", link: "/commands/if" },
              { text: "for", link: "/commands/for" },
              { text: "foreach", link: "/commands/foreach" },
              { text: "while", link: "/commands/while" },
              { text: "switch", link: "/commands/switch" },
              { text: "break", link: "/commands/break" },
              { text: "continue", link: "/commands/continue" },
            ],
          },
          {
            text: "Variables",
            collapsed: true,
            items: [
              { text: "set", link: "/commands/set" },
              { text: "unset", link: "/commands/unset" },
              { text: "append", link: "/commands/append" },
              { text: "incr", link: "/commands/incr" },
              { text: "global", link: "/commands/global" },
              { text: "variable", link: "/commands/variable" },
              { text: "upvar", link: "/commands/upvar" },
            ],
          },
          {
            text: "Lists",
            collapsed: true,
            items: [
              { text: "list", link: "/commands/list" },
              { text: "lappend", link: "/commands/lappend" },
              { text: "lassign", link: "/commands/lassign" },
              { text: "lindex", link: "/commands/lindex" },
              { text: "linsert", link: "/commands/linsert" },
              { text: "llength", link: "/commands/llength" },
              { text: "lmap", link: "/commands/lmap" },
              { text: "lrange", link: "/commands/lrange" },
              { text: "lrepeat", link: "/commands/lrepeat" },
              { text: "lreplace", link: "/commands/lreplace" },
              { text: "lreverse", link: "/commands/lreverse" },
              { text: "lsearch", link: "/commands/lsearch" },
              { text: "lset", link: "/commands/lset" },
              { text: "lsort", link: "/commands/lsort" },
            ],
          },
          {
            text: "Strings",
            collapsed: true,
            items: [
              { text: "string", link: "/commands/string" },
              { text: "concat", link: "/commands/concat" },
              { text: "join", link: "/commands/join" },
              { text: "split", link: "/commands/split" },
              { text: "format", link: "/commands/format" },
              { text: "scan", link: "/commands/scan" },
              { text: "subst", link: "/commands/subst" },
            ],
          },
          {
            text: "Dictionaries",
            collapsed: true,
            items: [
              { text: "dict", link: "/commands/dict" },
            ],
          },
          {
            text: "Procedures",
            collapsed: true,
            items: [
              { text: "proc", link: "/commands/proc" },
              { text: "apply", link: "/commands/apply" },
              { text: "return", link: "/commands/return" },
              { text: "tailcall", link: "/commands/tailcall" },
              { text: "uplevel", link: "/commands/uplevel" },
              { text: "rename", link: "/commands/rename" },
            ],
          },
          {
            text: "Exceptions",
            collapsed: true,
            items: [
              { text: "catch", link: "/commands/catch" },
              { text: "try", link: "/commands/try" },
              { text: "throw", link: "/commands/throw" },
              { text: "error", link: "/commands/error" },
            ],
          },
          {
            text: "Evaluation",
            collapsed: true,
            items: [
              { text: "eval", link: "/commands/eval" },
              { text: "expr", link: "/commands/expr" },
            ],
          },
          {
            text: "Introspection",
            collapsed: true,
            items: [
              { text: "info", link: "/commands/info" },
              { text: "trace", link: "/commands/trace" },
              { text: "namespace", link: "/commands/namespace" },
            ],
          },
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
