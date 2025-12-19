# Test: expr variable in comparison
set x 10
set y 20
puts [expr {$x < $y}]
puts [expr {$x == $y}]
puts [expr {$x != $y}]
puts [expr {$x >= 10}]
