# Test: info args - procedure with one argument
proc onearg {x} { return $x }
puts [info args onearg]
