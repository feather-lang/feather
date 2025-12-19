# Test: yield returns to coroutine caller
proc gen {} {
    yield
    yield "first"
    yield "second"
    return "third"
}
coroutine g gen
puts [g]
puts [g]
puts [g]
