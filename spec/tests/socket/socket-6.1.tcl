# Test: socket -server -reuseport option
catch {
    set srv [socket -server {apply {{chan addr port} {}}} -reuseport 1 0]
    puts [string match sock* $srv]
    close $srv
} result
# reuseport may not be supported on all systems
puts [expr {[string match sock* $result] || [string match "*not supported*" $result] || $result eq "1"}]
