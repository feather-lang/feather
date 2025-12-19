# Test: multiple coroutines with different names
proc showSelf {} {
    yield
    yield [info coroutine]
    return "done"
}
coroutine coro1 showSelf
coroutine coro2 showSelf
puts [coro1]
puts [coro2]
