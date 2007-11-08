#!/bin/sh
#
# Find system libraries, extract and rename various functions from
# them, and bundle into one file.  This is for the static case, and it
# works for both C and Fortran.  Fortran commonly uses a file
# (f90main.o) that links the C main to the Fortran MAIN_.
#
#  crt1.o:
#     U main -> monitor_faux_main
#
#  libc.a:
#     T _exit -> hide_u_exit
#
# $Id: copy_libs.sh,v 1.10 2007/08/03 19:30:38 krentel Exp krentel $
#
# Most Linux/GNU/gcc systems use:
#
#   CC=gcc
#   CC_OPTS='-static'
#   CRT_FILE=crt1.o
#
# Catamount (PGI C and Fortran) uses:
#
#   CC=cc   (or /opt/xt-pe/1.5.31/bin/snos64/cc)
#   CC_OPTS='--target=catamount'
#   CRT_FILE=cstart.o
#

CC=cc
CC_OPTS='--target=catamount'
CRT_FILE=cstart.o
VERBOSE='-v'

OUTPUT_FILE="monitor_sys_fcns.o"

die()
{
    echo ; echo "$1"
    exit 1
}

#
# Step 1 -- Get the gcc -v link line.
#
[ -f hello.c ] || die "no such file: hello.c"
rm -f hello hello.o
${CC} ${CC_OPTS} -c -o hello.o hello.c
[ $? -eq 0 ] || die "unable to make hello.o"
link_line=`${CC} ${CC_OPTS} ${VERBOSE} -o hello hello.o 2>&1 | grep hello | tail -1`
[ $? -eq 0 ] || die "unable to link hello.o"

#
# Step 2 -- Look for files directly on the link line.
#
for opt in $link_line
do
    if expr "$opt" : '.*'"$CRT_FILE" >/dev/null 2>&1 ; then
	if [ -f "$opt" ] && [ ! -f orig_${CRT_FILE} ]; then
	    cp -v "$opt" orig_${CRT_FILE}
	fi
    fi

    if expr "$opt" : '.*libc.a' >/dev/null 2>&1 ; then
	if [ -f "$opt" ] && [ ! -f orig_libc.a ]; then
	    cp -v "$opt" orig_libc.a
	fi
    fi
done

#
# Step 3 -- Parse the link line for search directories with the form
# -Ldir or -L dir.
#
search_path=
arg_is_dir=no
for opt in $link_line
do
    dir=
    if [ $arg_is_dir = yes ]; then
	dir="$opt"
	arg_is_dir=no
    else
	case $opt in
	    -L?* )
		dir=`echo $opt | sed 's/-L//'`
		;;
	    -L )
		arg_is_dir=yes
		;;
	esac
    fi
    if [ -n "$dir" ]; then
	if [ -d "$dir" ]; then
	    search_path="$search_path $dir"
	else
	    echo "not a directory: $dir" 1>&2
	fi
    fi
done

#
# Step 4 -- If not on the link line, then try the -L search
# directories and some standard directories.
#
for name in ${CRT_FILE} libc.a
do
    copy="orig_$name"
    if [ -f "$copy" ]; then
	continue
    fi

    # Try the -L search directories first.
    for dir in $search_path
    do
	if [ -f "${dir}/${name}" ]; then
	    cp -f -v "${dir}/${name}" "$copy"
	    break
	fi
    done
    if [ -f "$copy" ]; then
	continue
    fi

    # We really should have found the files by now, so it's a soft
    # error to get here.  As a last resort, try some standard library
    # directories.
    #
    echo "Warning ($0): unable to find $name in -L search path" 1>&2

    for dir in /lib* /usr/lib*
    do
        file=`find $dir -name $name 2>/dev/null | head -1`
	if [ -n "$file" ] && [ -f "$file" ]; then
	    cp -f -v "$file" "$copy"
	    break
	fi
    done
done

#
# Step 5 -- Objcopy individual files.
#
# crt1.o: main -> monitor_faux_main
#
[ -f orig_${CRT_FILE} ] || die "unable to find ${CRT_FILE}"

rename="objcopy --verbose --redefine-sym"
$rename main=monitor_faux_main orig_${CRT_FILE} faux_${CRT_FILE}
[ $? -eq 0 ] || die "unable to rename main in ${CRT_FILE}"

#
# libc.a: _exit -> hide_u_exit
#
[ -f orig_libc.a ] || die "unable to find libc.a"

nm_line=`nm -A orig_libc.a 2>/dev/null | grep -E -e "[TW] _exit" | head -1`
exit_file=`echo $nm_line | awk -F: '{ print $2 }'`

ar xv orig_libc.a "$exit_file"
[ $? -eq 0 ] || die "unable to extract $exit_file"

mv "$exit_file" orig_"$exit_file"
$rename _exit=hide_u_exit orig_"$exit_file" faux_"$exit_file"
[ $? -eq 0 ] || die "unable to rename _exit in $exit_file"

#
# Bundle them into a single .o file.
#
echo "ld faux_${CRT_FILE} faux_${exit_file} -> ${OUTPUT_FILE}"
ld -o "$OUTPUT_FILE" -r faux_${CRT_FILE} faux_${exit_file}
