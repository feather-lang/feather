# Test: info vars - pattern matching
set testvar1 1
set testvar2 2
set other 3
puts [lsort [info vars testvar*]]
