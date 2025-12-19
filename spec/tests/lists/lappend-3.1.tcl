# Test: lappend multiple values at once to new variable
lappend brand_new a b c d e
puts $brand_new
puts [llength $brand_new]
