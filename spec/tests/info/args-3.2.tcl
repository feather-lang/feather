# Test: info args - procedure with multiple arguments
proc multiargs {a b c} { return "$a $b $c" }
puts [info args multiargs]
