#!/bin/sh

./configure  \
    --prefix=`pwd`/install  \
    --with-libmonitor=/home/krentel/libmonitor-32/install  \
    --without-papi  \
    --with-symbols=/home/krentel/externs-syms  \
    --with-symtabAPI=/home/krentel/externs-syms

