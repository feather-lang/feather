# Test: array get with pattern
# Returns matching key-value pairs

array set items {x1 10 x2 20 y1 30 y2 40}
puts [lsort [array get items x*]]
