# Test: expr variable substitution
set a 5
set b 3
puts [expr {$a + $b}]
puts [expr {$a * $b}]
puts [expr {$a - $b}]
puts [expr {$a / $b}]
