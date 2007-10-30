#!/bin/sh
#

# Rename symbols from system libraries.

# Usage: cc -v --target=catamount file.o 2>&1 |
#            ./new_linker.sh config_file


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
    echo ; echo "*** Error: $1"
    exit 1
}

config_file="$1"
if [ "x$config_file" = x ]; then
    die "missing config file"
fi

#
# Step 1 -- Find the compiler's link line.
#
link_line=
while read line
do
    echo "$line"
    if expr "$line" : '.*static' >/dev/null 2>&1 ; then
	link_line="$line"
    fi
done
if [ "x$link_line" = x ]; then
    die "unable to find link line"
fi

echo ; echo
echo "Linker line:"
echo $link_line
echo


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

echo
echo "Search path:"
echo "$search_path"
echo


# Process lines from config file.
#
while read old_name new_name lib
do
    if expr "x${old_name}" : "x#" >/dev/null 2>&1 ; then
	continue
    fi
    echo
    echo "Working on ${old_name} -> ${new_name} in ${lib}"

    case "$lib" in
	*.o )
	    faux_name="faux_${lib}"
	    full_name=`echo $link_line | tr ' ' '\n' | grep -F $lib | head -1`
	    if [ "x$full_name" = x ]; then
		die "can't find $lib"
	    fi
	    if [ ! -f "$faux_name" ]; then
		cp -v "$full_name" "$faux_name" \
		    || die "unable to copy $full_name to $faux_name"
	    fi
	    ;;

	*.a )
	    for dir in $search_path
	    do
	        if [ -f "${dir}/${lib}" ]; then
		    cp -f -v "${dir}/${lib}" "$lib"
		    break
		fi
	    done
	    nm_line=`nm -A "${dir}/${lib}" 2>/dev/null | grep -E -e "[TW] $old_name" | head -1`
	    obj_file=`echo $nm_line | awk -F: '{ print $2 }'`
	    faux_name="faux_${obj_file}"
	    if [ ! -f "$faux_name" ]; then
		ar xv "$lib" "$obj_file"
		[ $? -eq 0 ] || die "unable to extract $obj_file"
		mv -f "$obj_file" "$faux_name"
	    fi
	    ;;

	* )
	    die "unknown library file: $lib"
	    ;;
    esac

    tmp_name="tmp_${faux_name}"
    rm -f "$tmp_name"
    mv -f "$faux_name" "$tmp_name"
    objcopy --verbose --redefine-sym "${old_name}=${new_name}" "$tmp_name" "$faux_name"
    [ $? -eq 0 ] || die "unable to objcopy"
done < "$config_file"


# Make linker.sh script.

script='linker_base.sh'
echo
echo "Making linker script: $script"

rm -f "$script"

cat >"$script" <<END_HEADER_1
#!/bin/sh

EXTRA_FILES=
OUTPUT_FILE=a.out
HPC_LIB_DIR=

END_HEADER_1


cat >>"$script" <<'END_HEADER_2'
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


echo "$link_line" | tr ' ' '\n' >>"$script"


cat >>"$script" <<'END_TRAILER'
END_OF_LINK_LINE

# Display and run the link line.
echo "$link_line" ; echo
${link_line}
END_TRAILER

chmod 755 "$script"

echo
echo "done"
