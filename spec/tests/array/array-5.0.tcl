# Test: array statistics returns info
# Just verify it doesn't error on valid array

array set data {a 1 b 2 c 3}
set stats [array statistics data]
puts [expr {[string length $stats] > 0}]
