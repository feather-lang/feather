# Test: uplevel command - level #0 refers to global
proc test {} {
    uplevel #0 {set result "from global"}
}
test
puts "result = $result"
