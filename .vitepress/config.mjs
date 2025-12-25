import { defineConfig } from "vitepress";

// https://vitepress.dev/reference/site-config
export default defineConfig({
  srcDir: "md",
  head: [["link", { rel: "icon", href: "/feather-logo.png" }]],

  title: "Feather Programming Language",
  description:
    "Feather is an embeddable TCL interpreter for adding interactive command shells to any application.",
  themeConfig: {
    logo: "/feather-logo.png",
    // https://vitepress.dev/reference/default-theme-config
    nav: [
      { text: "Home", link: "/" },
      { text: "Philosophy", link: "/philosophy" },
      { text: "Documentation", link: "/markdown-examples" },
    ],

    sidebar: [
      {
        text: "Getting Started",
        items: [
          { text: "Introduction", link: "/markdown-examples" },
          { text: "API Reference", link: "/api-examples" },
        ],
      },
      {
        text: "About",
        items: [
          { text: "Philosophy", link: "/philosophy" },
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
    }
  }
});
