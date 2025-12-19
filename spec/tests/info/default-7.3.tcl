# Test: info default - nonexistent procedure
catch {info default nonexistent x defval} err
puts $err
