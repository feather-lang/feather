# Test: coroutine with immediate yield to synchronize
proc counter {} {
    yield
    set i 0
    while {$i < 3} {
        yield $i
        incr i
    }
    return "done"
}
coroutine c counter
puts [c]
puts [c]
puts [c]
puts [c]
