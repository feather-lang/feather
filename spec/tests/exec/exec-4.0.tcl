# Test: exec background returns pid
# & at end runs in background

set pids [exec sleep 0 &]
puts [expr {[llength $pids] >= 1}]
