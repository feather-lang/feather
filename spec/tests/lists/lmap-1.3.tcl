# Test: lmap with continue
puts [lmap x {1 2 3 4 5} {if {$x == 3} continue; expr {$x * 2}}]
