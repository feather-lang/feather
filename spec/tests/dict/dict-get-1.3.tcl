# Test: dict get - error on missing key
set d [dict create a 1]
catch {dict get $d nonexistent} err
puts $err
