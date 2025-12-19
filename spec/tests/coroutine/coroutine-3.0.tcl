# Test: generator pattern - fibonacci
proc fib {} {
    yield
    set a 0
    set b 1
    while 1 {
        yield $a
        set tmp $b
        set b [expr {$a + $b}]
        set a $tmp
    }
}
coroutine fibonacci fib
fibonacci
puts [fibonacci]
puts [fibonacci]
puts [fibonacci]
puts [fibonacci]
puts [fibonacci]
puts [fibonacci]
puts [fibonacci]
