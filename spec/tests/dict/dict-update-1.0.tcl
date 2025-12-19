# Test: dict update - basic
set d [dict create a 1 b 2]
dict update d a x b y {
    puts "x=$x y=$y"
    set x 10
    set y 20
}
puts $d
