# Test: uplevel concatenates multiple args
proc test {} {
    set cmd "puts"
    set arg "hello"
    uplevel 1 $cmd $arg
}
test
