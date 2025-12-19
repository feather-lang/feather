# Test: info body - multi-line procedure
proc multiline {x} {
    set y [expr {$x + 1}]
    return $y
}
puts [info body multiline]
