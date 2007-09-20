#! /usr/local/bin/bash

grep -v '#' $1 | cut -f 1 -d ' ' | awk '{sum+=$1} END {print sum}'