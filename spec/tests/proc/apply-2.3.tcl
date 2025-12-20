# Test: apply - error when too many arguments passed
catch {apply {{x} {expr {$x * 2}}} 5 6 7} result
puts $result
