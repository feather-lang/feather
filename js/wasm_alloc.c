/**
 * Simple bump allocator for WASM builds.
 * Memory is imported from the host.
 */

extern unsigned char __heap_base;

static unsigned char *heap_ptr = &__heap_base;

void *alloc(unsigned int size) {
    unsigned char *ptr = heap_ptr;
    heap_ptr += size;
    // Align to 8 bytes
    heap_ptr = (unsigned char *)(((unsigned long)heap_ptr + 7) & ~7);
    return ptr;
}

void free(void *ptr) {
    // Bump allocator doesn't free
    (void)ptr;
}

// Comparison function type for list sorting
typedef int (*ListCmpFunc)(unsigned int interp, unsigned int a, unsigned int b, void *ctx);

// Helper to call a comparison function pointer - exported for JS to use
int wasm_call_compare(unsigned int interp, unsigned int a, unsigned int b,
                      ListCmpFunc fn, void *ctx) {
    return fn(interp, a, b, ctx);
}
