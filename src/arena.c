#include "arena.h"

#ifdef FEATHER_WASM_BUILD

extern unsigned char __heap_base;

static unsigned char *arena_base = &__heap_base;
static unsigned char *arena_ptr = &__heap_base;

void *feather_arena_alloc(size_t size) {
    unsigned char *ptr = arena_ptr;
    arena_ptr += size;
    /* Align to 8 bytes */
    arena_ptr = (unsigned char *)(((size_t)arena_ptr + 7) & ~7);
    return ptr;
}

void feather_arena_reset(void) {
    arena_ptr = arena_base;
}

size_t feather_arena_used(void) {
    return (size_t)(arena_ptr - arena_base);
}

size_t feather_arena_capacity(void) {
    return feather_arena_used();
}

/* Compatibility shims for existing code */
void *alloc(size_t size) {
    return feather_arena_alloc(size);
}

void free(void *ptr) {
    (void)ptr;
}

/* Comparison function type for list sorting */
typedef int (*ListCmpFunc)(unsigned int interp, unsigned int a, unsigned int b, void *ctx);

/* Helper to call a comparison function pointer - exported for JS to use */
int wasm_call_compare(unsigned int interp, unsigned int a, unsigned int b,
                      ListCmpFunc fn, void *ctx) {
    return fn(interp, a, b, ctx);
}

#endif /* FEATHER_WASM_BUILD */
