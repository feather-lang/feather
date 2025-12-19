# Test: dict update - add new variable creates key
set d [dict create a 1]
dict update d a x b y {
    set y 2
}
puts $d
