# Test: info procs - pattern matching
proc testproc1 {} {}
proc testproc2 {} {}
proc otherproc {} {}
puts [lsort [info procs testproc*]]
