# Test: uplevel returns result of evaluated script
proc test {} {
    uplevel 1 {expr {2 + 3}}
}
puts "result = [test]"
