# Test: info args - procedure with args parameter
proc varargs {x args} { return $x }
puts [info args varargs]
