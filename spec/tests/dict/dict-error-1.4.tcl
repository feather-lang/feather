# Test: dict incr - non-integer value
set d [dict create a hello]
catch {dict incr d a} err
puts $err
