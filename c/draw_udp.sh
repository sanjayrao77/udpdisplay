#!/bin/bash

./udpdisplay udp 192.168.1.100:8081 query
jpegtopnm "$1" | pnmscale -xysize 400 400 - | ./udpdisplay udp 192.168.1.100:8081 draw -
