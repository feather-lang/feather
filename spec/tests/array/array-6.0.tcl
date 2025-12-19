# Test: array set with odd number of elements
# Should error on invalid list

catch {array set data {a 1 b}} result
puts [string match "*must have an even*" $result]
