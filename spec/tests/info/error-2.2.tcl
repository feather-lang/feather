# Test: info exists with too many arguments
catch {info exists a b} err
puts $err
