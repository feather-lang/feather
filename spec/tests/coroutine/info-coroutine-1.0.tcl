# Test: info coroutine returns coroutine name
proc showName {} {
    yield
    yield [info coroutine]
    return "done"
}
coroutine myCoroutine showName
puts [myCoroutine]
puts [myCoroutine]
