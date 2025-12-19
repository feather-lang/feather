# Test: chan configure -encoding query
set enc [chan configure stdout -encoding]
puts [expr {$enc ne ""}]
