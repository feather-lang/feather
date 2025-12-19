# Test: info script - set and get script name
set original [info script]
info script "/tmp/test.tcl"
puts [info script]
info script $original
