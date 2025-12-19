# Test: regexp -nocase option
puts [regexp -nocase {foo} "FOOBAR"]
puts [regexp -nocase {foo} "FooBar"]
puts [regexp {foo} "FOOBAR"]
