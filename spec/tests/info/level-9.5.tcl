# Test: info level 0 - current command
proc showme {arg1 arg2} {
    puts [info level 0]
}
showme hello world
