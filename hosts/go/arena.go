package main

/*
#include <stdlib.h>
#include <string.h>
*/
import "C"
import (
	"sync"
	"unsafe"
)

// Arena provides bump-pointer allocation for parse/eval temporaries.
// Uses C memory to avoid CGO pointer issues.
type Arena struct {
	chunks []unsafe.Pointer // C-allocated chunks
	sizes  []int            // Size of each chunk
	offset int              // Current offset in last chunk
}

// Arena handle management
var (
	arenaMu      sync.RWMutex
	arenaHandles = make(map[uintptr]*Arena)
	nextArenaID  uintptr = 1
)

func allocArenaHandle(arena *Arena) uintptr {
	arenaMu.Lock()
	defer arenaMu.Unlock()
	id := nextArenaID
	nextArenaID++
	arenaHandles[id] = arena
	return id
}

func getArena(h uintptr) *Arena {
	arenaMu.RLock()
	defer arenaMu.RUnlock()
	return arenaHandles[h]
}

func freeArenaHandle(h uintptr) {
	arenaMu.Lock()
	arena := arenaHandles[h]
	delete(arenaHandles, h)
	arenaMu.Unlock()

	// Free all C chunks
	if arena != nil {
		for _, chunk := range arena.chunks {
			C.free(chunk)
		}
	}
}

const arenaChunkSize = 64 * 1024

// NewArena creates a new arena with C-allocated memory
func NewArena() *Arena {
	return &Arena{
		chunks: make([]unsafe.Pointer, 0),
		sizes:  make([]int, 0),
	}
}

// Alloc allocates memory from the arena (C memory)
func (a *Arena) Alloc(size, align int) unsafe.Pointer {
	if size == 0 {
		return nil
	}

	// Ensure we have a chunk
	if len(a.chunks) == 0 {
		chunkSize := arenaChunkSize
		if size > chunkSize {
			chunkSize = size
		}
		chunk := C.malloc(C.size_t(chunkSize))
		if chunk == nil {
			return nil
		}
		a.chunks = append(a.chunks, chunk)
		a.sizes = append(a.sizes, chunkSize)
		a.offset = 0
	}

	// Get current chunk info
	lastIdx := len(a.chunks) - 1
	chunkPtr := a.chunks[lastIdx]
	chunkSize := a.sizes[lastIdx]

	// Align the offset
	aligned := (a.offset + align - 1) &^ (align - 1)

	// Check if we need a new chunk
	if aligned+size > chunkSize {
		newChunkSize := arenaChunkSize
		if size > newChunkSize {
			newChunkSize = size
		}
		newChunk := C.malloc(C.size_t(newChunkSize))
		if newChunk == nil {
			return nil
		}
		a.chunks = append(a.chunks, newChunk)
		a.sizes = append(a.sizes, newChunkSize)
		a.offset = 0
		aligned = 0
		chunkPtr = newChunk
	}

	ptr := unsafe.Pointer(uintptr(chunkPtr) + uintptr(aligned))
	a.offset = aligned + size
	return ptr
}

// Strdup duplicates a string into the arena
func (a *Arena) Strdup(s string) *byte {
	if len(s) == 0 {
		return nil
	}

	ptr := a.Alloc(len(s)+1, 1)
	if ptr == nil {
		return nil
	}

	// Copy string data using C memcpy
	C.memcpy(ptr, unsafe.Pointer(C.CString(s)), C.size_t(len(s)+1))
	return (*byte)(ptr)
}

// Mark returns the current offset
func (a *Arena) Mark() int {
	return a.offset
}

// Reset resets to a previous mark
func (a *Arena) Reset(mark int) {
	if mark <= a.offset {
		a.offset = mark
	}
}

// CGO exports for arena operations

//export goArenaPush
func goArenaPush(ctxHandle C.uintptr_t) C.uintptr_t {
	arena := NewArena()
	return C.uintptr_t(allocArenaHandle(arena))
}

//export goArenaPop
func goArenaPop(ctxHandle C.uintptr_t, arenaHandle C.uintptr_t) {
	freeArenaHandle(uintptr(arenaHandle))
}

//export goArenaAlloc
func goArenaAlloc(arenaHandle C.uintptr_t, size, align C.size_t) unsafe.Pointer {
	arena := getArena(uintptr(arenaHandle))
	if arena == nil {
		return nil
	}
	return arena.Alloc(int(size), int(align))
}

//export goArenaStrdup
func goArenaStrdup(arenaHandle C.uintptr_t, s *C.char, length C.size_t) *C.char {
	arena := getArena(uintptr(arenaHandle))
	if arena == nil {
		return nil
	}

	// Allocate from arena (C memory)
	ptr := arena.Alloc(int(length)+1, 1)
	if ptr == nil {
		return nil
	}

	// Copy the string
	C.memcpy(ptr, unsafe.Pointer(s), length)
	*(*byte)(unsafe.Pointer(uintptr(ptr) + uintptr(length))) = 0

	return (*C.char)(ptr)
}

//export goArenaMark
func goArenaMark(arenaHandle C.uintptr_t) C.size_t {
	arena := getArena(uintptr(arenaHandle))
	if arena == nil {
		return 0
	}
	return C.size_t(arena.Mark())
}

//export goArenaReset
func goArenaReset(arenaHandle C.uintptr_t, mark C.size_t) {
	arena := getArena(uintptr(arenaHandle))
	if arena == nil {
		return
	}
	arena.Reset(int(mark))
}
