package main

/*
#include <stdlib.h>
#include <string.h>
*/
import "C"
import (
	"runtime/cgo"
	"unsafe"
)

// Arena provides bump-pointer allocation for parse/eval temporaries.
// Uses C memory to avoid CGO pointer issues.
type Arena struct {
	handle cgo.Handle
	chunks []unsafe.Pointer // C-allocated chunks
	sizes  []int            // Size of each chunk
	offset int              // Current offset in last chunk
}

// Handle returns the stable handle for this Arena
func (a *Arena) Handle() uintptr {
	if a == nil {
		return 0
	}
	return uintptr(a.handle)
}

// getArena retrieves an Arena by handle using cgo.Handle
func getArena(h uintptr) *Arena {
	if h == 0 {
		return nil
	}
	return cgo.Handle(h).Value().(*Arena)
}

// freeArena frees the Arena's handle and all C chunks
func freeArena(arena *Arena) {
	if arena == nil {
		return
	}
	// Free all C chunks
	for _, chunk := range arena.chunks {
		C.free(chunk)
	}
	arena.chunks = nil
	arena.sizes = nil
	if arena.handle != 0 {
		arena.handle.Delete()
		arena.handle = 0
	}
}

const arenaChunkSize = 64 * 1024

// NewArena creates a new arena with C-allocated memory
func NewArena() *Arena {
	a := &Arena{
		chunks: make([]unsafe.Pointer, 0),
		sizes:  make([]int, 0),
	}
	a.handle = cgo.NewHandle(a)
	return a
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

// MarkPacked returns the current state as a packed uint64 (chunkIndex << 32 | offset)
func (a *Arena) MarkPacked() uint64 {
	chunkIdx := len(a.chunks) - 1
	if chunkIdx < 0 {
		chunkIdx = 0
	}
	return uint64(chunkIdx)<<32 | uint64(a.offset)
}

// ResetPacked resets to a previous mark, freeing chunks allocated after the mark
func (a *Arena) ResetPacked(packed uint64) {
	chunkIdx := int(packed >> 32)
	offset := int(packed & 0xFFFFFFFF)

	// Free chunks allocated after mark
	for i := len(a.chunks) - 1; i > chunkIdx; i-- {
		C.free(a.chunks[i])
	}

	if chunkIdx >= 0 && chunkIdx < len(a.chunks) {
		a.chunks = a.chunks[:chunkIdx+1]
		a.sizes = a.sizes[:chunkIdx+1]
		a.offset = offset
	} else if len(a.chunks) == 0 {
		// No chunks yet, just reset offset
		a.offset = 0
	}
}

// CGO exports for arena operations

//export goArenaPush
func goArenaPush(ctxHandle C.uintptr_t) C.uintptr_t {
	arena := NewArena()
	return C.uintptr_t(arena.Handle())
}

//export goArenaPop
func goArenaPop(ctxHandle C.uintptr_t, arenaHandle C.uintptr_t) {
	arena := getArena(uintptr(arenaHandle))
	freeArena(arena)
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
	// Pack chunk index and offset into size_t
	// Since size_t is typically 64-bit, this should work
	return C.size_t(arena.MarkPacked())
}

//export goArenaReset
func goArenaReset(arenaHandle C.uintptr_t, mark C.size_t) {
	arena := getArena(uintptr(arenaHandle))
	if arena == nil {
		return
	}
	arena.ResetPacked(uint64(mark))
}
