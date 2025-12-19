# Test: info level - get command at specific level
proc inner {a b} {
    puts [info level 1]
}
proc outer {} {
    inner x y
}
outer
