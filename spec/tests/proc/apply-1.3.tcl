# Test: apply - anonymous function with default argument
puts [apply {{x {y 10}} {expr {$x + $y}}} 5]
