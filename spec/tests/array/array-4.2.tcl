# Test: array for with break
# Early exit from iteration

array set data {a 1 b 2 c 3 d 4}
set count 0
array for {k v} data {
    incr count
    if {$count == 2} break
}
puts $count
