# Test: info exists - global variable from within proc using :: prefix
set ::topvar 99
proc checkGlobal {} {
    puts [info exists ::topvar]
}
checkGlobal
