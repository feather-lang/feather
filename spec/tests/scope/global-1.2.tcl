# Test: global command - modify global variable
set x 100
proc test {} {
    global x
    set x 200
}
test
puts "x = $x"
