# Test: update idletasks - process idle callbacks
set x 0
after idle {set x 1}
update idletasks
puts $x
