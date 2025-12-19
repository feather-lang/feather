# Test: dict filter script - break
set d [dict create a 1 b 2 c 3 d 4 e 5]
puts [dict filter $d script {k v} {
    if {$v > 3} break
    expr {$v % 2 == 1}
}]
