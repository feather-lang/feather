# Test: apply - using upvar within lambda
proc testupvar {} {
    set x 10
    apply {{varname} {upvar 1 $varname v; set v 20}} x
    return $x
}
puts [testupvar]
