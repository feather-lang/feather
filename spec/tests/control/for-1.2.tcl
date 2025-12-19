# Test: for loop with break
for {set i 0} {$i < 10} {set i [expr $i + 1]} {
    if {$i == 3} {
        break
    }
    puts $i
}
