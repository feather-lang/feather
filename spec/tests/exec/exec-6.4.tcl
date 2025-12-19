# Test: exec binary data with -encoding
# Handle non-UTF8 output

puts [string length [exec printf "\x00\x01\x02"]]
