# Test: nested list creation
set inner [list a b c]
set outer [list $inner x y]
puts $outer
