# Test: yield with expression value
proc compute {} {
    yield
    yield [expr {2 + 2}]
    yield [expr {3 * 4}]
    return [expr {10 - 1}]
}
coroutine c compute
puts [c]
puts [c]
puts [c]
