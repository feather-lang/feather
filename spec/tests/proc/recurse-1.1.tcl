# Test: recursive procedure (factorial)
proc factorial {n} {
    if {$n <= 1} {
        return 1
    }
    return [expr $n * [factorial [expr $n - 1]]]
}
puts [factorial 5]
