# Test: local variables persist across yields
proc stateful {} {
    set count 0
    yield
    incr count
    yield $count
    incr count
    yield $count
    incr count
    return $count
}
coroutine s stateful
puts [s]
puts [s]
puts [s]
