# Test: uplevel command - basic usage
proc test {} {
    set x 10
    uplevel 1 {set y 20}
}
test
puts "y = $y"
