# Test: catch stores error message in variable
catch {error "something went wrong"} msg
puts $msg
