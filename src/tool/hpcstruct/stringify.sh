#!/bin/bash
#
# Read a text file and translate it line by line to the syntax of a
# multi-line C string.  For example:
#
#   all:  --->  "all:\n"
#

quote='"'
newline='\n'

IFS=

while read -r line
do
    printf "%s%s%s%s\n"  "$quote" "${line//\"/\\\"}" "$newline" "$quote"
done
