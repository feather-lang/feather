# Test: vwait returns empty string on simple form
set x ""
after 10 {set x done}
puts [vwait x]
