# Test: yield value with special characters
proc special {} {
    yield
    yield "hello world"
    yield {with {braces}}
    yield "with\ttab"
    return "done"
}
coroutine s special
puts [s]
puts [s]
puts [s]
puts [s]
