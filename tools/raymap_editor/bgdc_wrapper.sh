#!/bin/bash
export LD_LIBRARY_PATH="$1:$LD_LIBRARY_PATH"
"$1/bgdc" -i "$1" "$@"
