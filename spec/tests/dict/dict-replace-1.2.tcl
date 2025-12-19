# Test: dict replace - error on odd args
catch {dict replace {a 1} b} err
puts $err
