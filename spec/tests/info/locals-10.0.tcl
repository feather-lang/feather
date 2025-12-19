# Test: info locals - in procedure
proc testlocals {} {
    set a 1
    set b 2
    puts [lsort [info locals]]
}
testlocals
