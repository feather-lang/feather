# Test: dict map - continue skips entry
set d [dict create a 1 b 2 c 3]
puts [dict map {k v} $d {
    if {$k eq "b"} continue
    expr {$v * 10}
}]
