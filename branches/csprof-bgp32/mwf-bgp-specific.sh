#!/bin/sh

here=$(dirname $0)

$here/configure  \
    --prefix=$HOME/csprof-install \
    --with-libmonitor=/home/krentel/libmonitor-32/install  \
    --without-papi  \
    --with-symbols=/home/krentel/externs-syms  \
    --with-symtabAPI=/home/krentel/externs-syms
