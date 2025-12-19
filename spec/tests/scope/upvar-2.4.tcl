# Test: upvar #1 - absolute level
set x "global"
proc outer {} {
    set x "outer"
    inner
}
proc inner {} {
    upvar #1 x var
    puts "var = $var"
}
outer
