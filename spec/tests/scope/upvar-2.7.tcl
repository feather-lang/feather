# Test: upvar - implement decr from manual
proc decr {varName {decrement 1}} {
    upvar 1 $varName var
    incr var [expr {-$decrement}]
}
set counter 10
decr counter
puts "counter = $counter"
decr counter 3
puts "counter = $counter"
