# Test: dict map - basic transformation
set d [dict create a 1 b 2 c 3]
puts [dict map {k v} $d {expr {$v * 2}}]
