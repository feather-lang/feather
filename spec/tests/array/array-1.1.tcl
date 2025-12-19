# Test: array names with glob pattern
# Filter keys by pattern

array set data {foo_a 1 foo_b 2 bar_a 3 bar_b 4}
puts [lsort [array names data foo_*]]
