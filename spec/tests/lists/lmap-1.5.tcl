# Test: lmap multiple vars per list
puts [lmap {a b} {1 2 3 4 5 6} {expr {$a + $b}}]
