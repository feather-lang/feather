# Test: local variable scoping
set x 100
proc test {} {
    set x 1
    puts "inside: $x"
}
test
puts "outside: $x"
