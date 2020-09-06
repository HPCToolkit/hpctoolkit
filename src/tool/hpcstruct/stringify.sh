#!/bin/sh
#
# Read a text file and translate it line by line to the syntax of a
# multi-line C string.  For example:
#
#   all:  --->  "all:\n"
#

if ! test -z "$1"; then exec "$0" < "$1"; fi

quote='"'
newline='\n'

IFS=

while read -r line
do
    printf "%s%s%s%s\n"  "$quote" "$line" "$newline" "$quote"
done
