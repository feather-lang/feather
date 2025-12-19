# Test: uplevel for implementing control construct
proc repeat {n body} {
    for {set i 0} {$i < $n} {incr i} {
        uplevel 1 $body
    }
}
set sum 0
repeat 5 {
    incr sum
}
puts "sum = $sum"
