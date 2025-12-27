#!/usr/bin/env bun
import { $ } from "bun";

const REPO_PATH = "../feather";
const BRANCH = "main";

async function main() {
  // js/feather.js -> .vitepress/feather.js
  console.log("Extracting js/feather.js -> .vitepress/feather.js");
  await $`git -C ${REPO_PATH} show ${BRANCH}:js/feather.js > .vitepress/feather.js`;

  console.log("Extracting js/feather.js -> md/public/feather.js");
  await $`git -C ${REPO_PATH} show ${BRANCH}:js/feather.js > md/public/feather.js`;

  // js/feather.wasm -> md/public/feather.wasm
  console.log("Extracting js/feather.wasm -> md/public/feather.wasm");
  await $`git -C ${REPO_PATH} show ${BRANCH}:js/feather.wasm > md/public/feather.wasm`;

  // testcases/ -> testcases/
  console.log("Extracting testcases/ -> testcases/");
  await $`rm -rf testcases`;
  await $`git -C ${REPO_PATH} archive ${BRANCH} testcases/ | tar -x`;

  console.log("Done!");
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
