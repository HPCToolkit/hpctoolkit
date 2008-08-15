#!/bin/sh
#
#  Download and unpack a tarfile using wget.
#
#  $Id$
#
url="$1"
tarfile="$2"

die()
{
    echo "$0: $*"
    exit 1
}

#
# Select directory to hold tarfiles as: DISTDIR environ variable,
# or else distfiles directory.
#
if test ! -d "$DISTDIR" ; then
    DISTDIR=../distfiles
fi

#
# Step 1 -- See if tarfile is in DISTDIR and download if not.
#
orig_dir=`pwd`
mkdir -p "$DISTDIR"

case "$url" in
    ftp* | http* )
	;;
    * )
	die "unknown url type: $url"
	;;
esac

if test "x$tarfile" = x ; then
    tarfile=`expr "$url" : '.*/\(.*\)'`
fi
test "x$tarfile" != x || die "unable to parse: $url"

if test ! -f "${DISTDIR}/${tarfile}" ; then
    if type wget >/dev/null 2>&1 ; then
	cd "$DISTDIR" || die "unable to cd: $DISTDIR"
	echo "wget $url"
	wget "$url"
	test $? -eq 0 || die "wget failed"
    else
	die "unable to find wget"
    fi
fi

#
# Step 2 -- Unpack the tarfile.
#
cd "$orig_dir" || die "unable to cd: $orig_dir"

case "$tarfile" in
    *.tar.gz | *.tgz )
	echo tar xvzf "${DISTDIR}/${tarfile}"
	tar xvzf "${DISTDIR}/${tarfile}"
	test $? -eq 0 || die "unable to untar: $tarfile"
	;;
    * )
	die "unknown file format: $tarfile"
	;;
esac

