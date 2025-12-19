# Test: while with break
set i 0
while {1} {
    if {$i >= 3} {
        break
    }
    puts $i
    set i [expr $i + 1]
}
