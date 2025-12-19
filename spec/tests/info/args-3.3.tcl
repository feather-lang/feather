# Test: info args - procedure with default argument
proc withdefault {x {y 10}} { return "$x $y" }
puts [info args withdefault]
