# Test: dict incr - new key starts at 0
set d [dict create]
puts [dict incr d newkey]
puts [dict incr d another 5]
puts $d
