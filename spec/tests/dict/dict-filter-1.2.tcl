# Test: dict filter script - basic
set d [dict create a 1 b 2 c 3 d 4]
puts [dict filter $d script {k v} {expr {$v > 2}}]
