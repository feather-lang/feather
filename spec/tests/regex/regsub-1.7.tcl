# Test: regsub return value with varName (count of replacements)
set count [regsub -all {o} "foobar" "X" result]
puts $count
puts $result
