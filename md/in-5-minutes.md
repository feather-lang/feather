---
outline: deep
---

# Feather in 5 Minutes

If you have been around long enough to remember TCL, or curious enough
to have taken a look for no good reason, you should feel right at home.

Feather _is_ TCL in the sense that it chooses the same names and
building blocks for the problems solved by TCL.  This also makes it a
good fit for LLMs, as they can leverage their existing training data.

If you are new: TCL is really simple.

## Syntax

```tcl
# at the start of a command, # introduces a comment
set port 8080
```

Commands are lists of word.  In this example `set` got the words
`port` and `8080` as arguments, defining the variable `port` to hold
the word `8080`.

Commands are separated by spaces, and line breaks separate commands:

```tcl
set host localhost
puts "Listening on $host:$port"
```

You can add `"` around your word to prevent the spaces from breaking it up.

In such double quoted strings, `$` *substitutes* value.  The example above prints:

```
Listenining on localhost:8080
```

because `localhost` was assigned to the `host` variable, and `8080` to the `port` variable.

To prevent substitutions in words, use braces: `{`, and `}`:

```tcl
puts {Listening on $host:$port}
```

prints

```
Listenining on $host:$port
```

Now for the last important rule: to get the value *of another command*, you need to use `[` and `]`.


```tcl
dict set config baseURL "https://www.feather-lang.dev"
puts [dict get $config]/in-5-minutes.html

# Output: https://www.feather-lang.dev/in-5-minutes.html
```

## On Values and Types

Feather, just like TCL, does not assign a type to a value.

Individual commands are free to interpret words however they wish.

When Feather needs to treat a value as a list, number, or dictionary
it performs the conversion once and remembers the representation,
until the same value is interpreted again as something else.

This allows Feather to save work, while still staying malleable like clay:

```tcl
if {$port < 1024} {
  error "Reserved port"
}
```

The command `if` is a regular command that decides to treat its first word
as an expression (`$port < 1024` here), and its second word as a script to
execute in case the expression evaluated to `1`.

It is _not_ special syntax.

In fact, you can easily define your own language building blocks using
`uplevel`, which evaluates its argument as a script in another
context:

```tcl
# to get this syntax
set such Feather
set much wow
repeat 3 { puts "Such $such. Much $much." }

# we only need to define this
proc repeat {times script} {
  for {set i 0} {$i < $times} {incr i} {
    uplevel 1 $script
  }
}
```

Here we interpret whatever is held in `script` as code to run, and run
it in the context where `repeat` is called.

Now, all that's left is learning about Feather's built-in commands.
