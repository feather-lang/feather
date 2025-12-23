#ifndef INCLUDE_WASM_EXPORTS
#define INCLUDE_WASM_EXPORTS

#ifdef __wasm__
#define TCLC_EXPORT __attribute__((visibility("default"))) __attribute__((used))
#else
#define TCLC_EXPORT
#endif

#endif
