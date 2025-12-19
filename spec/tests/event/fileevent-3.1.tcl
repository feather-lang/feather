# Test: fileevent - invalid channel
catch {fileevent nonexistent readable} result
puts $result
