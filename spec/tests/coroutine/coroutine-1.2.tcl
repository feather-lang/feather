# Test: yield returns value passed by caller
proc echo {} {
    yield
    while 1 {
        set val [yield "received"]
        puts "got: $val"
    }
}
coroutine e echo
e
puts [e "first"]
puts [e "second"]
