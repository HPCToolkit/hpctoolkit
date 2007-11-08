#!/bin/sh 

sleep 1
hostname
dd if=/dev/zero of=/dev/null count=1000000
exit 0
