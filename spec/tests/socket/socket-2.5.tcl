# Test: socket -server sockname returns address info
set srv [socket -server {apply {{chan addr port} {}}} 0]
set info [chan configure $srv -sockname]
puts [expr {[llength $info] >= 3}]
close $srv
