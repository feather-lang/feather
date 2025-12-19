# Test: deleting coroutine with rename
proc counter {} {
    set i 0
    while 1 {
        yield [incr i]
    }
}
coroutine c counter
puts [c]
puts [c]
rename c {}
puts "deleted"
