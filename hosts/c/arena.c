/*
 * arena.c - Arena Allocator for C Host
 *
 * Simple bump-pointer arena with LIFO push/pop semantics.
 */

#include "../../core/tclc.h"
#include <stdlib.h>
#include <string.h>

/* Arena chunk size */
#define ARENA_CHUNK_SIZE (64 * 1024)

/* Arena chunk */
typedef struct ArenaChunk {
    struct ArenaChunk *next;
    size_t             used;
    size_t             size;
    char               data[];
} ArenaChunk;

/* Arena state */
typedef struct Arena {
    ArenaChunk *current;    /* Current chunk */
    ArenaChunk *chunks;     /* List of all chunks */
} Arena;

/* Arena stack for push/pop */
#define MAX_ARENA_DEPTH 32

typedef struct ArenaStack {
    Arena *arenas[MAX_ARENA_DEPTH];
    int    top;
} ArenaStack;

/* Global arena stack (one per context, but we use a simple global for now) */
static ArenaStack globalArenaStack = {0};

/* Allocate a new chunk */
static ArenaChunk *newChunk(size_t minSize) {
    size_t size = minSize > ARENA_CHUNK_SIZE ? minSize : ARENA_CHUNK_SIZE;
    ArenaChunk *chunk = malloc(sizeof(ArenaChunk) + size);
    if (!chunk) return NULL;

    chunk->next = NULL;
    chunk->used = 0;
    chunk->size = size;
    return chunk;
}

/* Push a new arena onto the stack */
void *hostArenaPush(void *ctx) {
    (void)ctx;

    if (globalArenaStack.top >= MAX_ARENA_DEPTH) {
        return NULL;
    }

    Arena *arena = calloc(1, sizeof(Arena));
    if (!arena) return NULL;

    globalArenaStack.arenas[globalArenaStack.top++] = arena;
    return arena;
}

/* Pop arena from stack and free all memory */
void hostArenaPop(void *ctx, void *arenaPtr) {
    (void)ctx;

    Arena *arena = arenaPtr;
    if (!arena) return;

    /* Free all chunks */
    ArenaChunk *chunk = arena->chunks;
    while (chunk) {
        ArenaChunk *next = chunk->next;
        free(chunk);
        chunk = next;
    }

    /* Remove from stack */
    for (int i = globalArenaStack.top - 1; i >= 0; i--) {
        if (globalArenaStack.arenas[i] == arena) {
            /* Shift remaining elements down */
            for (int j = i; j < globalArenaStack.top - 1; j++) {
                globalArenaStack.arenas[j] = globalArenaStack.arenas[j + 1];
            }
            globalArenaStack.top--;
            break;
        }
    }

    free(arena);
}

/* Allocate from arena with alignment */
void *hostArenaAlloc(void *arenaPtr, size_t size, size_t align) {
    Arena *arena = arenaPtr;
    if (!arena || size == 0) return NULL;

    /* Get or create current chunk */
    ArenaChunk *chunk = arena->current;
    if (!chunk) {
        chunk = newChunk(size);
        if (!chunk) return NULL;
        arena->chunks = chunk;
        arena->current = chunk;
    }

    /* Align the current position */
    size_t aligned = (chunk->used + align - 1) & ~(align - 1);

    /* Check if we need a new chunk */
    if (aligned + size > chunk->size) {
        chunk = newChunk(size);
        if (!chunk) return NULL;
        chunk->next = arena->chunks;
        arena->chunks = chunk;
        arena->current = chunk;
        aligned = 0;
    }

    void *ptr = chunk->data + aligned;
    chunk->used = aligned + size;
    return ptr;
}

/* Duplicate string into arena */
char *hostArenaStrdup(void *arena, const char *s, size_t len) {
    char *dup = hostArenaAlloc(arena, len + 1, 1);
    if (dup) {
        memcpy(dup, s, len);
        dup[len] = '\0';
    }
    return dup;
}

/* Get current position (mark) for reset */
size_t hostArenaMark(void *arenaPtr) {
    Arena *arena = arenaPtr;
    if (!arena || !arena->current) return 0;
    return arena->current->used;
}

/* Reset to a previous mark */
void hostArenaReset(void *arenaPtr, size_t mark) {
    Arena *arena = arenaPtr;
    if (!arena || !arena->current) return;

    /* Only reset if mark is in current chunk */
    if (mark <= arena->current->used) {
        arena->current->used = mark;
    }
}
