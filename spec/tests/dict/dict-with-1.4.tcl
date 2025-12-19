# Test: dict with - empty dict
set d [dict create]
dict with d {
    set a 1
}
puts $d
