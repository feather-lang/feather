# Test: global variable modifications persist
set counter 0
proc increment {} {
    global counter
    incr counter
}
increment
increment
increment
puts "counter = $counter"
