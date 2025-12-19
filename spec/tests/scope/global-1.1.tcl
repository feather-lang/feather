# Test: global command - basic usage
set x 100
proc test {} {
    global x
    puts "x = $x"
}
test
