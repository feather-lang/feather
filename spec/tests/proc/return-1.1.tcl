# Test: return value from procedure
proc double {x} {
    return [expr $x * 2]
}
puts [double 5]
puts [double 21]
