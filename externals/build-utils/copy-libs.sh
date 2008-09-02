#!/bin/sh
#
#  Copy files from stage_prefix to PREFIX or install_prefix.
#  Run this script from the build-utils directory.
#
#  $Id: copy-libs.sh 1556 2008-08-18 16:45:21Z krentel $
#

options_file=../options.conf

copy_list='
lib/libdwarf.so
lib/libelf.so
lib/libmonitor.so
lib/libmonitor_wrap.a
lib/libxml2.so
lib/libcommon.so
lib/libsymtabAPI.so
lib/libxerces-c.so
lib/libxerces-depdom.so
lib/libxed.a
'

die()
{
    echo "$0: $*"
    exit 1
}

#
# The options file defines stage_prefix and install_prefix.
# Use PREFIX if defined.
#
test -f "$options_file" || die "unable to find: $options_file"
. "$options_file"

if test -d "$PREFIX" ; then
    install_prefix="$PREFIX"
fi

echo "stage directory: $stage_prefix"
echo "install prefix:  $install_prefix"

dir="${install_prefix}/lib"
mkdir -p "$dir" || die "unable to mkdir: $dir"

for file in $copy_list
do
    #
    # The easy case is when the file is right where we think it is.
    #
    if test -f "${stage_prefix}/${file}" ; then
	echo "copy: $file"
	cp -f "${stage_prefix}/${file}" "${install_prefix}/${file}"
    else
	#
	# Maybe the file is in some other directory.
	#
	name=`basename "$file"`
	f=`find "$stage_prefix" -name "$name" 2>/dev/null | head -1`
	if test -f "$f" ; then
	    echo "copy: $file"
	    cp -f "$f" "${install_prefix}/${file}"
	else
	    echo "missing: $file"
	fi
    fi
done

