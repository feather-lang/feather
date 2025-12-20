# Test: apply - nested apply calls
puts [apply {x {apply {y {expr {$y * 2}}} [expr {$x + 1}]}} 5]
