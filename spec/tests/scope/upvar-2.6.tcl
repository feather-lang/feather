# Test: upvar - implement add2 from manual
proc add2 {name} {
    upvar $name x
    set x [expr {$x + 2}]
}
set val 10
add2 val
puts "val = $val"
