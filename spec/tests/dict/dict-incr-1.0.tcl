# Test: dict incr - basic increment
set d [dict create count 5]
puts [dict incr d count]
puts $d
