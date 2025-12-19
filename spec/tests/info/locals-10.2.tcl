# Test: info locals - pattern matching
proc testpattern {} {
    set foo1 1
    set foo2 2
    set bar 3
    puts [lsort [info locals foo*]]
}
testpattern
