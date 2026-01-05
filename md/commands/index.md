# Built-in Commands

Feather provides a comprehensive set of built-in commands that implement Feather's core functionality.
The commands listed here mostly match the commands implemented by TCL.

## Subtractions from TCL

- Feather does not support arrays, because they were essentially supplanted by dicts.
- There are no built-in I/O functions provided by Feather, as this is a responsibilty of the host.
- There are no OO functions provided, because these encourage programming in the large.

## Additions to TCL

- The [info](./info) command supports `type` and `methods` subcommands
  to get access to the internal representation of a value, and the
  methods exposed on foreign objects.
- The [usage](./usage) command provides declarative CLI argument parsing
  with automatic help generation.


## Control Flow

Commands for branching and looping:

- [if](./if) - Conditional execution
- [for](./for) - C-style loop
- [foreach](./foreach) - Iterate over list elements
- [while](./while) - Conditional loop
- [switch](./switch) - Multi-way branching
- [break](./break) - Exit a loop
- [continue](./continue) - Skip to next iteration

## Variables

Commands for managing variables:

- [set](./set) - Get or set variable value
- [unset](./unset) - Delete variables
- [append](./append) - Append to variable
- [incr](./incr) - Increment integer variable
- [global](./global) - Access global variables
- [variable](./variable) - Declare namespace variables
- [upvar](./upvar) - Reference variables in other frames

## Lists

Commands for list manipulation:

- [list](./list) - Create a list
- [lappend](./lappend) - Append to list variable
- [lassign](./lassign) - Assign list elements to variables
- [lindex](./lindex) - Get list element by index
- [linsert](./linsert) - Insert elements into list
- [llength](./llength) - Get list length
- [lmap](./lmap) - Map expression over list
- [lrange](./lrange) - Extract sublist
- [lrepeat](./lrepeat) - Create repeated list
- [lreplace](./lreplace) - Replace list elements
- [lreverse](./lreverse) - Reverse list order
- [lsearch](./lsearch) - Search list for element
- [lset](./lset) - Set list element
- [lsort](./lsort) - Sort list

## Strings

Commands for string manipulation:

- [string](./string) - String operations (length, index, range, match, etc.)
- [concat](./concat) - Concatenate arguments
- [join](./join) - Join list elements into string
- [split](./split) - Split string into list
- [format](./format) - Printf-style formatting
- [scan](./scan) - Parse string with format
- [subst](./subst) - Perform substitutions

## Dictionaries

Commands for dictionary (key-value) operations:

- [dict](./dict) - Dictionary operations (create, get, set, exists, keys, etc.)

## Procedures

Commands for defining and calling procedures:

- [proc](./proc) - Define a procedure
- [apply](./apply) - Apply anonymous function (lambda)
- [return](./return) - Return from procedure
- [tailcall](./tailcall) - Tail call optimization
- [uplevel](./uplevel) - Execute in caller's frame
- [rename](./rename) - Rename a command

## Exceptions

Commands for error handling:

- [catch](./catch) - Catch errors
- [try](./try) - Advanced exception handling
- [throw](./throw) - Throw exception
- [error](./error) - Raise error

## Evaluation

Commands for evaluating expressions and scripts:

- [eval](./eval) - Evaluate script
- [expr](./expr) - Evaluate mathematical expression

## Introspection

Commands for examining the interpreter state:

- [info](./info) - Query interpreter information
- [trace](./trace) - Trace variable and command execution
- [namespace](./namespace) - Manage namespaces

## Argument Parsing

Commands for parsing command-line arguments:

- [usage](./usage) - Declarative CLI argument parsing with help generation
