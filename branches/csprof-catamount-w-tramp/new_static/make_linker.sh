#!/bin/sh
#
# Run the compiler and output a linker script for the static link
# case.
#
# $Id: make_linker.sh,v 1.4 2007/08/03 19:30:38 krentel Exp krentel $
#
# Most Linux/GNU/gcc systems use:
#
#   COMPILER=gcc
#   COMPILER_OPTIONS='-static'
#   REPLACE_FILES='crt1.o'
#
# Catamount C uses:
#
#   COMPILER=cc   (or /opt/xt-pe/1.5.31/bin/snos64/cc)
#   COMPILER_OPTIONS='--target=catamount'
#   REPLACE_FILES='cstart.o'
#
# Catamount Fortran uses:
#
#   COMPILER=ftn  (or /opt/xt-pe/1.5.31/bin/snos64/ftn)
#   COMPILER_OPTIONS='--target=catamount'
#   REPLACE_FILES='cstart.o'
#

COMPILER=cc
COMPILER_OPTIONS='--target=catamount'
VERBOSE='-v'
HELLO_SOURCE='hello.c'

REPLACE_FILES='cstart.o'
HPC_LIB_DIR=`/bin/pwd`
HPC_FILES='monitor_sys_fcns.o static_main.o'

die()
{
    echo ; echo "$1"
    exit 1
}

#
# Step 1 -- Get the compiler link line.
#
[ -f "$HELLO_SOURCE" ] || die "no such file: $HELLO_SOURCE"
rm -f hello hello.o
${COMPILER} ${COMPILER_OPTIONS} -c -o hello.o "$HELLO_SOURCE"
[ $? -eq 0 ] || die "unable to make hello.o"
link_line=`${COMPILER} ${COMPILER_OPTIONS} ${VERBOSE} -o hello hello.o 2>&1 | grep hello | tail -1`
[ $? -eq 0 ] || die "unable to link hello.o"

#
# Step2 -- Linker script header.
#
cat <<END_HEADER_1
#!/bin/sh

EXTRA_FILES=
OUTPUT_FILE=a.out
HPC_LIB_DIR='${HPC_LIB_DIR}'
END_HEADER_1

echo
cat <<'END_HEADER_2'
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
END_HEADER_2

#
# Step 3 -- Copy the compiler's link line, comment-out the
# REPLACE_FILES and insert our own.
#
is_output_arg=no
output_arg_done=no
for arg in $link_line
do
    is_replace_file=no
    for f in $REPLACE_FILES
    do
        if expr "x$arg" : "x.*$f" >/dev/null 2>&1 ; then
	    is_replace_file=yes
	    break
	fi
    done

    if [ "$is_replace_file" = yes ]; then
	echo "# $arg"

    elif [ "x$arg" = "x-o" ]; then
	is_output_arg=yes

    elif [ "$is_output_arg" = yes ]; then
	echo '-o ${OUTPUT_FILE}'
	is_output_arg=no
	output_arg_done=yes

    elif [ "x$arg" = "xhello.o" ]; then
	for f in $HPC_FILES
	do
	    echo '${HPC_LIB_DIR}'/"$f"
	done
	if [ "$output_arg_done" != yes ]; then
	    echo '-o ${OUTPUT_FILE}'
	fi
	echo '${EXTRA_FILES}'
	echo '$@'

    else
	echo "x$arg" | sed -e 's/^x//'
    fi
done

#
# Step 4 -- Link script trailer.
#
cat <<'END_TRAILER'
END_OF_LINK_LINE

# Display and run the link line.
echo "$link_line" ; echo
${link_line}
END_TRAILER
