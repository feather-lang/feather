# Test: generator pattern - infinite sequence
proc evens {} {
    yield
    set i 0
    while 1 {
        yield $i
        incr i 2
    }
}
coroutine nextEven evens
nextEven
puts [nextEven]
puts [nextEven]
puts [nextEven]
puts [nextEven]
puts [nextEven]
