# Test: dict for - break
set d [dict create a 1 b 2 c 3 d 4]
dict for {k v} $d {
    puts "$k=$v"
    if {$k eq "b"} break
}
puts "after break"
