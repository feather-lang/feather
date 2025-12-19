# Test: exec with << input redirection
# Pass string as stdin

puts [exec cat << "input text"]
