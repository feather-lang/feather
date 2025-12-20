# Test: apply - error when too few arguments passed
catch {apply {{x y} {expr {$x + $y}}} 5} result
puts $result
