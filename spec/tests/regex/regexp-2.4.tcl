# Test: regexp anchors
puts [regexp {^foo} "foobar"]
puts [regexp {^foo} "barfoo"]
puts [regexp {bar$} "foobar"]
puts [regexp {bar$} "barfoo"]
