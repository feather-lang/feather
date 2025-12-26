#ifndef FEATHER_ARENA_H
#define FEATHER_ARENA_H

#include <stddef.h>

/**
 * Arena-based memory allocation for feather.
 *
 * In WASM builds, all allocations come from a single bump arena.
 * The arena is reset after each top-level eval, reclaiming all memory.
 *
 * WARNING: feather_arena_reset() invalidates ALL pointers from previous
 * allocations. Only call at top-level eval boundaries.
 *
 * Native builds may provide their own allocator via FeatherHostOps,
 * or use the default arena if available.
 */

/* Allocate `size` bytes from the current arena. Returns aligned pointer. */
void *feather_arena_alloc(size_t size);

/* Reset the arena, reclaiming all allocated memory. */
void feather_arena_reset(void);

/* Get current arena usage in bytes (for diagnostics). */
size_t feather_arena_used(void);

/* Get total arena capacity in bytes. */
size_t feather_arena_capacity(void);

#endif /* FEATHER_ARENA_H */
