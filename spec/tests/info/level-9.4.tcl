# Test: info level - negative level (relative)
proc inner {} {
    puts [info level -1]
}
proc outer {x y} {
    inner
}
outer a b
