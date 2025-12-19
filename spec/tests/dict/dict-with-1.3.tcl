# Test: dict with - add new variable creates key
set d [dict create a 1]
dict with d {
    set b 2
}
puts $d
