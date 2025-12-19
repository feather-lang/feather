# Test: dict update - unset variable removes key
set d [dict create a 1 b 2]
dict update d a x b y {
    unset x
}
puts $d
