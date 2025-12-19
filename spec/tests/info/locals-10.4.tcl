# Test: info locals - excludes global linked variables
set ::gvar 100
proc testglobal {} {
    global gvar
    set localvar 1
    puts [lsort [info locals]]
}
testglobal
