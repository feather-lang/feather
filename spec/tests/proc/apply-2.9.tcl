# Test: apply - map example from documentation
proc map {lambda list} {
    set result {}
    foreach item $list {
        lappend result [apply $lambda $item]
    }
    return $result
}
puts [map {x {expr {$x * $x}}} {1 2 3 4 5}]
