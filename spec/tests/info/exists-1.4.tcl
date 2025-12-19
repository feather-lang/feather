# Test: info exists - nonexistent local variable in proc
proc testNonexistent {} {
    puts [info exists undefined]
}
testNonexistent
