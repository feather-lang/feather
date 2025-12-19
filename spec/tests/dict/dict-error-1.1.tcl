# Test: dict - unknown subcommand
catch {dict unknown arg} err
puts $err
