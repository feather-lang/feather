# Test: apply - anonymous function with explicit return
puts [apply {x {return [expr {$x * 3}]}} 7]
