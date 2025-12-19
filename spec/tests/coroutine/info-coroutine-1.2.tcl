# Test: info coroutine in nested proc inside coroutine
proc inner {} {
    return [info coroutine]
}
proc outer {} {
    yield
    yield [inner]
    return "done"
}
coroutine test123 outer
puts [test123]
puts [test123]
