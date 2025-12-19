# Test: coroutine that returns normally without yield
proc noYield {} {
    return 42
}
puts [coroutine ny noYield]
