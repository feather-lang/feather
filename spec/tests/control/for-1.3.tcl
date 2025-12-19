# Test: for loop with continue
for {set i 0} {$i < 5} {set i [expr $i + 1]} {
    if {$i == 2} {
        continue
    }
    puts $i
}
