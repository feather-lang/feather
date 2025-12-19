# Test: array unset removes elements
array set data {x 1 y 2 z 3}
puts [array size data]
array unset data
puts [array size data]
