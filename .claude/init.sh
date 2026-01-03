#!/usr/bin/env bash

if [ "$CLAUDE_CODE_REMOTE" != true ]; then
  exit 0
fi

curl https://mise.run | sh
mise trust
mise install
