# Test: coroutine with args passed to initial proc
proc greet {name} {
    yield
    yield "hello $name"
    return "bye $name"
}
coroutine g greet "world"
puts [g]
puts [g]
