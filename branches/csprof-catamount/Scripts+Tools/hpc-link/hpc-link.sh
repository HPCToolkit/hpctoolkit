#!/bin/sh
#
# Usage: hpc-link [linker flags] compiler arg ...
#
# $Id$
#

#HPC_INSTALL_DIR=.
#HPC_WRAP_NAMES=_exit
#HPC_INSERT_FILES=hpc_insert.o

## MWF change to accomodate csprof

HPC_INSTALL_DIR=`dirname $0`
HPC_WRAP_NAMES='main _exit malloc realloc calloc sigaction signal sigprocmask'
HPC_INSERT_FILES='monitor.o libcsprof.o'
CC=gcc

insert_dir="${HPC_INSTALL_DIR}/libexec"

die()
{
    echo "Error: $@" 1>&2
    exit 1
}

usage()
{
    cat <<EOF
Usage: hpc-link [linker flags] compiler arg ...
EOF
    exit 0
}

#
# Write C array of nm addresses from "binary" into "file".
# If binary doesn't exist, then write dummy array.
#
make_nm_file()
{
    binary="$1"
    file="$2"

    rm -f "$file"
    cat >"$file" <<EOF
unsigned long csprof_nm_addrs[] = {
EOF

    if test -f "$binary" ; then
	nm -v "$binary" |
	awk '$2 == "T" || $2 == "t" || $2 == "W" { print "  0x" $1 ",  /* " $3 " */" }' >>"$file"
    else
	echo "  0x0," >>"$file"
    fi

    cat >>"$file" <<EOF
};
int csprof_nm_addrs_len = sizeof(csprof_nm_addrs) / sizeof(csprof_nm_addrs[0]);
EOF
}

#
# Step 1 -- Parse linker options.
#
verbose=no
while test "x$1" != x
do
    case "$1" in
	-c | --compiler )
	    test "x$2" != x || die "missing argument: $@"
	    CC="$2"
	    shift ; shift
	    ;;

	-d | --install-directory )
	    test "x$2" != x || die "missing argument: $@"
	    HPC_INSTALL_DIR="$2"
	    shift ; shift
	    ;;

	-h | --help )
	    usage
	    ;;

	-i | --insert )
	    test "x$2" != x || die "missing argument: $@"
	    HPC_INSERT_FILES="${HPC_INSERT_FILES} $2 "
	    shift ; shift
	    ;;

	-v | --verbose )
	    verbose=yes
	    shift
	    ;;

	-w | --wrap )
	    test "x$2" != x || die "missing argument: $@"
	    HPC_WRAP_NAMES="${HPC_WRAP_NAMES} $2 "
	    shift ; shift
	    ;;

	-- )
	    shift
	    break
	    ;;

	* )
	    break
	    ;;
    esac
done

#
# Must have a compiler/linker command and at least one argument.
#
test "x$1" != x || die "missing compiler/linker command"
test "x$2" != x || die "no arguments"

compiler="$1"
shift

#
# See if link line includes -o, but don't change the line.
#
out_file=a.out
now=no
for arg in $@
do
    if test "$now" = yes ; then
	out_file="$arg"
	break
    elif test "$arg" = "-o" ; then
	now=yes
    fi
done

#
# Step 2 -- Make wrapper and insert file options.
#
wrap_options=
for name in $HPC_WRAP_NAMES
do
    wrap_options="${wrap_options} -Wl,--wrap,${name} "
done

insert_files=
for f in $HPC_INSERT_FILES
do
    file="${insert_dir}/$f"
    test -f "$file" || die "missing insert file: $file"
    insert_files="${insert_files} ${file} "
done

if test "$verbose" = yes ; then
    echo "wrap names:   " $HPC_WRAP_NAMES
    echo "insert files: " $HPC_INSERT_FILES
    echo "output file:  " $out_file
    echo "original command line: " $compiler "$@"
fi

#
# Step 3 -- Link with dummy nm file.
#
empty_c=__hpc_dummy_nm.c
empty_o=__hpc_dummy_nm.o
rm -f "$empty_c" "$empty_o"

make_nm_file __hpc_not_exist__ "$empty_c"
args="-c -o $empty_o $empty_c"
$CC $args || die "compile failed:" $CC $args

args="$wrap_options $@ $insert_files $empty_o"
if test "$verbose" = yes ; then
    echo "first (dummy nm) command line: " $compiler $args
fi
$compiler $args || die "link failed:" $compiler $args

#
# Step 4 -- Run the nm script on the first binary to get the real nm
# file.
#
full_c=__hpc_real_nm.c
full_o=__hpc_real_nm.o
rm -f "$full_c" "$full_o"

make_nm_file "$out_file" "$full_c"
args="-c -o $full_o $full_c"
$CC $args || die "compile failed:" $CC $args

#
# Step 5 -- Link with real nm file.
#
args="$wrap_options $@ $insert_files $full_o"
if test "$verbose" = yes ; then
    echo "second (real nm) command line: " $compiler $args
fi
$compiler $args || die "link failed:" $compiler $args

#
# Step 6 -- Clean up temp files.
# Leave the nm data file when verbose is set.
#
rm -f "$empty_c" "$empty_o" "$full_o"
if test "$verbose" != yes ; then
    rm -f "$full_c"
fi

exit 0
