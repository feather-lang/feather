# Test: info commands - pattern matching
proc testcmd1 {} {}
proc testcmd2 {} {}
puts [lsort [info commands testcmd*]]
