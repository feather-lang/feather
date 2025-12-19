# Test: dict update - missing key
set d [dict create a 1]
dict update d a x missing y {
    puts "x=$x"
    puts [info exists y]
}
puts $d
