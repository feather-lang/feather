# Test: array for basic iteration
# Iterate over key-value pairs

array set colors {red 1 green 2 blue 3}
set result {}
array for {k v} colors {
    lappend result "$k=$v"
}
puts [lsort $result]
