# Test: procedure with multiple arguments
proc add {a b} {
    puts [expr $a + $b]
}
add 3 4
add 10 20
