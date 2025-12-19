# Test: chan configure returns options list for stdout
set opts [chan configure stdout]
puts [expr {[llength $opts] > 0}]
