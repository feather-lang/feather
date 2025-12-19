# Test: uplevel #1 - absolute level
set x "global"
proc outer {} {
    set x "outer"
    inner
}
proc inner {} {
    set x "inner"
    uplevel #1 {puts "x = $x"}
}
outer
