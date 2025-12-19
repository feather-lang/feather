# Test: update - process pending events
set x 0
after 0 {set x 1}
update
puts $x
