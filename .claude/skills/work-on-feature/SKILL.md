---
name: work-on-feature
description: |
  Use this skill when asked to work on a feature.  Features are listed in features.yaml
  This skill describes the working process you must follow.
---

# How to work on a feature

Run bin/harness prompt <feature> and follow the instructions

In order to make the tests pass, you will have to make changes to the C host implementation.

Leverage glib-2.0 for the implementation.

In particular, regular expressions will always be the regular expressions of the host language.
This is an intentional divergence from real TCL.

If no tests exist yet for the feature, use the add-tests skill.

To get into the mood, before you start, review the manual for the command your implementing using the view-manual skill.

Every builtin command should live in a separate file: core/builtin_<command>.c

It might happen that you cannot implement the desired functionality in the core given the functions exposed by the TclHost interface.  In this case you must flag this to the user immediately and STOP.
