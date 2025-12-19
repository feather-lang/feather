# Test: error in conditional - error not reached
if {0} {
    error "this should not happen"
}
puts "ok"
