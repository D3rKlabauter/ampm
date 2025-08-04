#!/bin/bash
cd "$(dirname "$0")/../samples/openFrameworks/bin" || exit 1
startx ./openFrameworks -- -s off
