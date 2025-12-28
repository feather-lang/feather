package main

import (
	"fmt"
	"os"
	"runtime"

	"github.com/feather-lang/feather"
)

// memStats holds memory statistics for a point in time
type memStats struct {
	alloc      uint64 // bytes allocated and still in use
	totalAlloc uint64 // bytes allocated (even if freed)
	sys        uint64 // bytes obtained from system
	numGC      uint32 // number of completed GC cycles
}

func getMemStats() memStats {
	runtime.GC() // Force GC to get accurate measurement
	var m runtime.MemStats
	runtime.ReadMemStats(&m)
	return memStats{
		alloc:      m.Alloc,
		totalAlloc: m.TotalAlloc,
		sys:        m.Sys,
		numGC:      m.NumGC,
	}
}

func (m memStats) String() string {
	return fmt.Sprintf("Alloc: %6d KB, TotalAlloc: %6d KB, Sys: %6d KB, NumGC: %d",
		m.alloc/1024, m.totalAlloc/1024, m.sys/1024, m.numGC)
}

func main() {
	interp := feather.New()
	defer interp.Close()

	const iterations = 10000
	const reportInterval = 1000

	// Get baseline memory stats
	startMem := getMemStats()
	fmt.Println("Start:", startMem)

	// Run stress test: repeatedly create objects, procs, variables
	// This should not leak memory in the Go implementation
	script := `
		set x [list a b c d e f g h i j]
		lappend x k l m n o p q r s t
		proc tmp {} { return [expr {1 + 2}] }
		tmp
		rename tmp {}
	`

	for i := 0; i < iterations; i++ {
		_, err := interp.Eval(script)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Eval error at iteration %d: %v\n", i, err)
			os.Exit(1)
		}

		if i%reportInterval == 0 && i > 0 {
			stats := getMemStats()
			fmt.Printf("Iteration %5d: %s\n", i, stats)
		}
	}

	// Get final memory stats
	endMem := getMemStats()
	fmt.Println("End:  ", endMem)

	// Check for memory leaks
	// Calculate growth metrics
	allocGrowth := int64(endMem.alloc) - int64(startMem.alloc)
	allocGrowthKB := allocGrowth / 1024
	bytesPerIteration := float64(allocGrowth) / float64(iterations)

	fmt.Printf("\nMemory growth: %d KB (%.2f bytes/iteration)\n", allocGrowthKB, bytesPerIteration)

	// Threshold: Check bytes per iteration rather than absolute growth.
	// With proper cleanup, each iteration should not add unbounded memory.
	// Allow up to 50 bytes/iteration for some runtime overhead and GC lag,
	// but anything higher indicates a leak.
	const maxBytesPerIter = 50.0

	if bytesPerIteration > maxBytesPerIter {
		fmt.Fprintf(os.Stderr, "FAIL: Memory leak detected\n")
		fmt.Fprintf(os.Stderr, "  Start Alloc:        %d KB\n", startMem.alloc/1024)
		fmt.Fprintf(os.Stderr, "  End Alloc:          %d KB\n", endMem.alloc/1024)
		fmt.Fprintf(os.Stderr, "  Growth:             %d KB\n", allocGrowthKB)
		fmt.Fprintf(os.Stderr, "  Bytes/iteration:    %.2f (threshold: %.2f)\n", bytesPerIteration, maxBytesPerIter)
		fmt.Fprintf(os.Stderr, "\nThis indicates memory is growing unbounded over iterations.\n")
		fmt.Fprintf(os.Stderr, "Expected behavior: temporary allocations should be cleaned up after each eval.\n")
		os.Exit(1)
	}

	fmt.Println("PASS: No memory leaks detected")
}
