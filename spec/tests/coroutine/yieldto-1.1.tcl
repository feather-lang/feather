# Test: yieldto with string cat to receive multiple values
proc multiReceive {} {
    yield
    set args [yieldto string cat "ready"]
    return "received: $args"
}
coroutine mr multiReceive
puts [mr]
puts [mr a b c]
