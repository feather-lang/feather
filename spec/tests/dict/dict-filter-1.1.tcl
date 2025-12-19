# Test: dict filter value - basic pattern matching
set d [dict create a red b green c blue d red]
puts [dict filter $d value red]
puts [dict filter $d value *e*]
