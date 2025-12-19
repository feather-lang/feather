# Test: upvar level 2 - skip one frame
set x "global"
proc outer {} {
    set x "outer"
    inner
}
proc inner {name} {
    upvar 2 $name var
    set var "modified"
}
proc middle {} {
    inner x
}
outer
puts "x = $x"
