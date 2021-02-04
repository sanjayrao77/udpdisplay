#!/bin/bash

./udpdisplay tcp 192.168.1.100:8081 query
jpegtopnm "$1" | pnmscale -xysize 400 400 - | pnmtojpeg - | ./udpdisplay tcp 192.168.1.100:8081 draw+0+0 -
