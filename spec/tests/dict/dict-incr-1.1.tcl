# Test: dict incr - with explicit increment
set d [dict create count 5]
puts [dict incr d count 10]
puts [dict incr d count -3]
puts $d
