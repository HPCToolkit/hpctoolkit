#!/bin/sh
#
#  Copyright (c) 2007-2021, Rice University.
#  See the file LICENSE for details.
#
#  Smoke test of hpcrun/struct/prof on selection sort.
#

sort="`realpath "$1"`"
hpcrun="`realpath "$2"`"
hpcstruct="`realpath "$3"`"
hpcprof="`realpath "$4"`"

tmpdir="`mktemp -dp . test-sort.tmp.XXXXXXXX`"
trap "cd \"$PWD\" && rm -rf "'"$tmpdir"' EXIT
cd "$tmpdir"

ulimit -c 0
ulimit -t 60
ulimit -m 8000000
ulimit -v 8000000

#------------------------------------------------------------

ok() {
  RET=$?
  test $RET -eq 0 && echo "ok $@" || echo "not ok $@"

  if test $RET -ne 0; then
    echo '  ---'
    echo '  stderr: |'
    sed -e 's/^$/./;s/^/    /' cmd.stderr
    echo '  stdout: |'
    sed -e 's/^$/./;s/^/    /' cmd.stdout
    eval "msg$1"
    echo '  ...'

    echo "# FAIL $@" >&2
    cat cmd.stderr >&2
    echo >&2
  fi

  return $RET
}
bail() {
  echo "Bail out! $@"
  exit 0
}

measure=hpctoolkit-sort-measurements
database=hpctoolkit-sort-database
struct=sort.hpcstruct

#------------------------------------------------------------

echo TAP version 13
echo 1..3

#------------------------------------------------------------

msg1() {
  if find "$measure" -maxdepth 1 -type f -name '*.log' 2>- | grep -q .; then
    echo '  logs:'
    for log in "$measure"/*.log; do
      echo "    `basename "$log"`: |"
      sed -e 's/^$/./;s/^/      /' "$log"
    done
  fi
}

set -- -e REALTIME@5000 -t -o "$measure" "$sort"
$MESON_EXE_WRAPPER $hpcrun "$@" > cmd.stdout 2> cmd.stderr

ok 1 "hpcrun $@" || bail "Measurements were not collected successfully"

#------------------------------------------------------------

skipstruct=''
msg2() { :; }

set -- -j 4 --time -o "$struct" "$sort"
$MESON_EXE_WRAPPER $hpcstruct "$@" > cmd.stdout 2> cmd.stderr

ok 2 "hpcstruct $@" || skipstruct=Y

#------------------------------------------------------------

msg3() { :; }

set -- -S "$struct" -o "$database" "$measure"
if test -n "$skipstruct"; then shift 2; fi
$MESON_EXE_WRAPPER $hpcprof "$@" > cmd.stdout 2> cmd.stderr

ok 3 "hpcprof $@"

exit 0
