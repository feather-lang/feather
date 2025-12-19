# Test: basic while loop
set i 0
while {$i < 3} {
    puts $i
    set i [expr $i + 1]
}
