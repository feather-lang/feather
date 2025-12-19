# Test: info globals - from within proc
set myglobal 100
proc checkglobals {} {
    puts [expr {"myglobal" in [info globals]}]
}
checkglobals
