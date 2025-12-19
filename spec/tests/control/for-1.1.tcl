# Test: basic for loop
for {set i 0} {$i < 5} {set i [expr $i + 1]} {
    puts $i
}
