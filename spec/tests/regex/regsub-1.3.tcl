# Test: regsub -nocase option
puts [regsub -nocase {foo} "FOOBAR" "xxx"]
puts [regsub -nocase {foo} "FooBar" "xxx"]
