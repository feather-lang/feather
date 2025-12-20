# Test: apply - local variable in lambda body
puts [apply {x {
    set y [expr {$x + 1}]
    expr {$y * 2}
}} 4]
