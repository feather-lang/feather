---
outline: deep
---

# Introduction

Command languages are forgotten treasures: they've become so successful that they
have become invisible.

You are using one every time you type a command into your terminal, no matter what
shell is behind it.

There is certain immediacy and closeness to the material when you can pass a few
unstructured words to the computer and watch it carry out instructions.

You cannot put much logic into this, but rather have to combine
existing functionality in novel ways.

Interactive iteration is the name of the game: you come up with a command,
run it, watch the output, tweak it until you are satisified.

And then you either add it to your collection of scripts, or throw it away.

Coincidentally, this is a nudge toward good software design: how amazing is it,
that you can access any program on your computer through the *same interface*.

The hundres of thousands of lines of code implementing Postgres are at your finger tips.

So are Ruby, Python, node.js, a calculator, data sync utilities, and many more.

Feather aims to bring this way of interacting with software back: to
make it scriptable by users, without headaches for the developers; to
give developers the means to *talk* to their running applications – or
have AI agents do the same.

## TCL - the Tool Command Language

This idea of a small embeddable command language already exists in the form of TCL.

TCL had its brief moment in the spotlight when the Internet was just about to grow explosively.

It had humble beginnings and the same goal: provide easy interactive access to large and performant
C codebases.

Eventually Perl, then Ruby, and finally Python overtook it in the web development niche.

It's still alive and kicking in other domains, reasonably fast and has a usable standard library.

However, cross-compiling TCL with all its platform specific code and
the ambition to write entire applications in it, makes embedding in 2025 difficult.

You'll have to invest significant effort into picking the parts that
you want to keep for your usecase.

## Feather

Feather is the spiritual successor to TCL, aiming to bring joy to developers,
AI agents, and end users alike.

For developers, it makes embedding easy: it's an empty canvas,
leveraging the host's infrastructure for everything.  Feather takes
care of control structures, runtime semantics, and providing a rich
set of builtin commands for doing useful things within the universe of
a Feather program.

For AI agents, Feather allows for short and frequent feedback loops.
We all have seen what agents can do when they are allowed to remote
control a browser, but why stop there?  Giving agents access to a
Feather REPL in your running program is high-quality feedback.

For end users, Feather provides the simplest possible syntax of "Do this"
with very few rules to learn, allowing some form of customization.
Software "off the rack" fits everyone, but nobody well.  Creators
cannot cater to every desire – user customization offers a way to make more people happy.
