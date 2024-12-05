#!/bin/sh
cd `dirname $0`
#open-stage-control  -s localhost:18080 -t ./theme-contrast.css -l ./teleco-OSC.json
open-stage-control -s localhost:18080 -l ./teleco-OSC.json --custom-module ./detect_connect.js
# --read-only
