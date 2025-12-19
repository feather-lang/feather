# Test: info level - two levels deep
proc level2 {} {
    puts [info level]
}
proc level1 {} {
    level2
}
level1
