# Test: array anymore returns 0 when done
# Check completion status

array set nums {x 1}
set sid [array startsearch nums]
puts [array anymore nums $sid]
array nextelement nums $sid
puts [array anymore nums $sid]
array donesearch nums $sid
