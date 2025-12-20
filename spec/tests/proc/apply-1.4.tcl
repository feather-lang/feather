# Test: apply - anonymous function with default argument overridden
puts [apply {{x {y 10}} {expr {$x + $y}}} 5 20]
