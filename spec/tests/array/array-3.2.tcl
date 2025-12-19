# Test: array donesearch returns empty string
# Cleanup search state

array set data {a 1}
set sid [array startsearch data]
puts "[array donesearch data $sid]."
