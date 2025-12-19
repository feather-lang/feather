# Test: regexp with no match - variables should be empty
set result [regexp {xyz} "foobar" match]
puts $result
if {[info exists match]} { puts "match=$match" } else { puts "match unset" }
