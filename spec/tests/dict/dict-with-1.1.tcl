# Test: dict with - nested path
set d [dict create outer [dict create x 1 y 2]]
dict with d outer {
    puts "x=$x y=$y"
    set x 10
}
puts [dict get $d outer x]
