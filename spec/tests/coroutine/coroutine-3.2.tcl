# Test: coroutine using apply
coroutine counter apply {{} {
    yield
    set n 0
    while 1 {
        yield [incr n]
    }
}}
counter
puts [counter]
puts [counter]
puts [counter]
