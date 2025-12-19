# Test: uplevel command - access caller's variables
set x 100
proc test {} {
    uplevel 1 {puts "x = $x"}
}
test
