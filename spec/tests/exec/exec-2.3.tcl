# Test: exec with << multiline input
# Pass multiline string as stdin

puts [exec wc -l << "line1
line2
line3"]
