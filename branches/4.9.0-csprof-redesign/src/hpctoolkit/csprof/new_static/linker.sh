#!/bin/sh

EXTRA_FILES=
OUTPUT_FILE=a.out
HPC_LIB_DIR='/autofs/spin/home/krentel/new_static'

# Recombine the link args into a single line.
link_line=
while read arg
do
    if expr "x$arg" : "x#" >/dev/null 2>&1 ; then : ; else
        link_line="${link_line}${arg}  "
    fi
done <<END_OF_LINK_LINE
#
# This is the link line, edit if needed.
#
/usr/bin/ld
-m
elf_x86_64
-dynamic-linker
/lib64/ld-linux-x86-64.so.2
/opt/pgi/7.0.5/linux86-64/7.0-5/lib/pgi.ld
-L/opt/xt-mpt/1.5.52/mpich2-64/P2/lib
-L/opt/acml/3.6/pgi64/lib
-L/opt/xt-libsci/1.5.52/pgi/cnos64/lib
-L/opt/xt-mpt/1.5.52/sma/P2/lib
-L/opt/xt-tools/iobuf/1.0.5/lib/cnos64
-L/opt/xt-lustre-ss/1.5.52/lib64
-L/opt/xt-catamount/1.5.52/lib/cnos64
-L/opt/xt-libc/1.5.52/amd64/lib
-L/opt/xt-pe/1.5.52/lib/cnos64
-L/opt/xt-os/1.5.52/lib/cnos64
-L/opt/xt-service/1.5.52/lib/cnos64
-L/opt/pgi/7.0.5/linux86-64/7.0/lib/cray/catamount
-L/opt/pgi/7.0.5/linux86-64/7.0/lib
-L/opt/gcc/3.2.3/lib/gcc-lib/x86_64-suse-linux/3.2.3/
-L/opt/xt-pe/1.5.52/lib/cnos64
-Bstatic
-u
_start
-e
_start
-T/opt/xt-pe/1.5.52/lib/cnos64/ldscripts/app.lds
/opt/xt-pe/1.5.52/lib/cnos64/crt0_amd64.o
/opt/xt-pe/1.5.52/lib/cnos64/crtbegin.o
# /opt/xt-pe/1.5.52/lib/cnos64/cstart.o
-u
_sysio_lustre_init
${HPC_LIB_DIR}/monitor_sys_fcns.o
${HPC_LIB_DIR}/static_main.o
-o ${OUTPUT_FILE}
${EXTRA_FILES}
$@
--start
-liobuf
-lsci
-lacml
-lqk_pgftnrtl
-lsma
-lmpich
-llustre
-lqk_pgc
-lm
-lcatamount
-lsysio
-lportals
-lqk_C
-lc
-lgcc
--end
-lcrtend
-o ${OUTPUT_FILE}
END_OF_LINK_LINE

# Display and run the link line.
echo "$link_line" ; echo
${link_line}
