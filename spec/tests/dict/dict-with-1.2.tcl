# Test: dict with - unset removes key
set d [dict create a 1 b 2]
dict with d {
    unset a
}
puts $d
