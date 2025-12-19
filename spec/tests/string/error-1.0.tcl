# Test: string command errors
catch {string} msg
puts $msg
catch {string unknown} msg
puts $msg
catch {string length} msg
puts $msg
