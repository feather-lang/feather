# Test: unknown option error
catch {socket -badoption localhost 8080} result
puts [string match "*bad option*" $result]
