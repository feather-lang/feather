# Test: dict map - break
set d [dict create a 1 b 2 c 3 d 4]
puts [dict map {k v} $d {
    if {$v > 2} break
    expr {$v * 10}
}]
