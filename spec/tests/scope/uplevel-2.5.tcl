# Test: uplevel 0 - execute in current frame
proc test {} {
    set x 10
    uplevel 0 {set x 20}
    puts "x = $x"
}
test
