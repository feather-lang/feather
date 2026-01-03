#!/usr/bin/env bash

if [ "$CLAUDE_CODE_REMOTE" != true ]; then
  exit 0
fi

curl https://mise.run | sh
mise trust
mise install

# Install TCL 9 for oracle (reference TCL interpreter)
TCL_VERSION="9.0.1"
TCL_BUILD_DIR="/tmp/tcl-build-$$"
mkdir -p "$TCL_BUILD_DIR"
cd "$TCL_BUILD_DIR"
wget --no-check-certificate -O tcl-src.tar.gz "https://github.com/tcltk/tcl/archive/refs/tags/core-9-0-1.tar.gz"
tar xzf tcl-src.tar.gz
cd tcl-core-9-0-1/unix
./configure --prefix=/usr/local
make -j4
make install
ln -sf /usr/local/bin/tclsh9.0 /usr/local/bin/tclsh
ldconfig
cd /
rm -rf "$TCL_BUILD_DIR"

# Build the oracle binary
cd "$OLDPWD"
mise run build:oracle
