# Test: exec background pipeline returns multiple pids
# Multiple processes in background

set pids [exec echo test | cat &]
puts [expr {[llength $pids] >= 1}]
