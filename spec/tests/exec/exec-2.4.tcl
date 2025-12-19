# Test: exec pipeline with << input
# Combine input redirection with pipe

puts [exec tr a-z A-Z << "hello"]
