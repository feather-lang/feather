# Test: array names returns all keys
# Lists all element names in an array

array set colors {red 1 green 2 blue 3}
puts [lsort [array names colors]]
