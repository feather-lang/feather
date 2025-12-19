# Test: coroutine with accumulator pattern
coroutine accumulator apply {{} {
    set x 0
    while 1 {
        incr x [yield $x]
    }
}}
puts [accumulator 5]
puts [accumulator 10]
puts [accumulator 3]
