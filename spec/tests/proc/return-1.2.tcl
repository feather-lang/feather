# Test: implicit return (last command result)
proc square {x} {
    expr $x * $x
}
puts [square 3]
puts [square 7]
