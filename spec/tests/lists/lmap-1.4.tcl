# Test: lmap with break
puts [lmap x {1 2 3 4 5} {if {$x == 3} break; expr {$x * 2}}]
