#!/bin/sh

bib2xhtml="$1"
output="$2"
shift ; shift

"$bib2xhtml" -s unsort -d pubs-overview "$@" "$output" || exit $?
"$bib2xhtml" -s unsort -d pubs-hpctoolkit-a "$@" "$output" || exit $?
"$bib2xhtml" -s unsort -d pubs-hpctoolkit-b "$@" "$output" || exit $?
"$bib2xhtml" -s unsort -d pubs-prediction "$@" "$output" || exit $?
