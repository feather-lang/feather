# Test: coroutine with multiple args
proc adder {a b} {
    yield [expr {$a + $b}]
    return "done"
}
coroutine add adder 10 20
puts [add]
