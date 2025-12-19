# Test: info exists - local variable in proc
proc testLocal {} {
    set localvar 10
    puts [info exists localvar]
}
testLocal
