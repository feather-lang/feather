# Test: info with unknown subcommand
catch {info unknown} err
puts $err
