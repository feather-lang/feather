# Test: file system returns list
set sys [file system /tmp]
puts [expr {[llength $sys] >= 1}]
