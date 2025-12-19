# Test: nested procedure calls
proc double {x} {
    return [expr $x * 2]
}
proc quadruple {x} {
    return [double [double $x]]
}
puts [quadruple 3]
