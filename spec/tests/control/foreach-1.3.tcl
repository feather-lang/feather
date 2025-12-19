# Test: foreach with break
foreach x {a b c d e} {
    if {$x == "c"} {
        break
    }
    puts $x
}
