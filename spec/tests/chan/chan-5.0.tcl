# Test: chan flush stdout
chan puts -nonewline stdout "flushed"
chan flush stdout
chan puts stdout ""
