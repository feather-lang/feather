# Test: regexp -start option
puts [regexp -start 3 {foo} "foofoofoo"]
puts [regexp -start 3 {foo} "foobarfoo"]
regexp -start 3 {foo} "foobarfoo" match
puts $match
