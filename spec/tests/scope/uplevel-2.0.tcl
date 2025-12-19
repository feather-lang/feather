# Test: uplevel level 2 - skip one frame
set x "global"
proc outer {} {
    set x "outer"
    inner
}
proc inner {} {
    uplevel 2 {puts "x = $x"}
}
outer
