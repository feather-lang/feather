# Test: dict - not a valid dict
catch {dict size "a b c"} err
puts $err
