# Test: array for with empty array
# Should not execute body

array set empty {}
set count 0
array for {k v} empty {
    incr count
}
puts $count
