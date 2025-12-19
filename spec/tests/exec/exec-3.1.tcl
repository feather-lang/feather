# Test: exec errorcode on failure
# Check CHILDSTATUS in errorcode

catch {exec sh -c "exit 42"} result options
set code [dict get $options -errorcode]
puts [lindex $code 0]
puts [lindex $code 2]
