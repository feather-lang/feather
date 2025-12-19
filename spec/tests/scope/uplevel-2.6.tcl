# Test: nested uplevel calls
proc level3 {} {
    uplevel 1 {set z 30}
}
proc level2 {} {
    set y 20
    level3
    uplevel 1 {set w 40}
}
proc level1 {} {
    set x 10
    level2
}
level1
puts "z = $z"
puts "w = $w"
