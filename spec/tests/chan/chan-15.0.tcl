# Test: error on invalid channel
catch {chan close invalid_channel} err
puts [string match "*invalid*" $err]
