# Test: global command - multiple variables
set a 1
set b 2
set c 3
proc test {} {
    global a b c
    puts "a=$a b=$b c=$c"
}
test
