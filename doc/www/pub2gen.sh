#!/bin/sh

awk="$1"
header="$2"
footer="$3"
shift; shift; shift

exec "$awk" \
  -v varhdr="`cat "$header" | tr '\n' ' '`" \
  -v varftr="`cat "$footer" | tr '\n' ' '`" \
  "BEGIN {}
  /@header-hpctoolkit@/ { printf \"%s\", varhdr ; next; }
  /@footer-hpctoolkit@/ { printf \"%s\", varftr ; next; }
  { print; }
  END {}"
  "$@"
