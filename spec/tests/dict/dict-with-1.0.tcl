# Test: dict with - basic
set d [dict create a 1 b 2 c 3]
dict with d {
    puts "a=$a b=$b c=$c"
    set a 10
}
puts $d
