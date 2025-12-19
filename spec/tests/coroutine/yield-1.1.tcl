# Test: yield inside loop
proc numbers {n} {
    yield
    for {set i 1} {$i <= $n} {incr i} {
        yield $i
    }
    return "done"
}
coroutine nums numbers 5
puts [nums]
puts [nums]
puts [nums]
puts [nums]
puts [nums]
puts [nums]
