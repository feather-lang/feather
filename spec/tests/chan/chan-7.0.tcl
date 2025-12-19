# Test: chan tell on stdout
set pos [chan tell stdout]
puts [expr {$pos >= 0 || $pos == -1}]
