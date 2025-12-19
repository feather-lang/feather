# Test: info locals - includes proc arguments
proc testargs {x y} {
    set z 3
    puts [lsort [info locals]]
}
testargs 1 2
