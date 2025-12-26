/**
 * Stress test for arena-based memory management.
 * Verifies that memory is properly reclaimed after each eval.
 */

import { createFeather } from './feather.js';

async function stressTest() {
  const feather = await createFeather('./feather.wasm');
  const interp = feather.create();
  
  const iterations = 1000;
  const startMem = feather.memoryStats(interp);
  console.log('Start:', startMem);
  
  for (let i = 0; i < iterations; i++) {
    feather.eval(interp, `
      set x [list a b c d e f g h i j]
      lappend x k l m n o p q r s t
      proc tmp {} { return [expr {1 + 2}] }
      tmp
      rename tmp {}
    `);
    
    if (i % 100 === 0) {
      console.log(`Iteration ${i}:`, feather.memoryStats(interp));
    }
  }
  
  const endMem = feather.memoryStats(interp);
  console.log('End:', endMem);
  
  // Verify no significant growth
  if (endMem.scratchHandles > startMem.scratchHandles + 100) {
    console.error('FAIL: Handle leak detected');
    console.error(`  Start handles: ${startMem.scratchHandles}`);
    console.error(`  End handles: ${endMem.scratchHandles}`);
    process.exit(1);
  }
  if (endMem.wasmArenaUsed > 10000) {
    console.error('FAIL: WASM arena not being reset');
    console.error(`  Arena used: ${endMem.wasmArenaUsed}`);
    process.exit(1);
  }
  
  console.log('PASS: No memory leaks detected');
}

stressTest().catch(e => {
  console.error('Error:', e);
  process.exit(1);
});
