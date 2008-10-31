#
#  Configure include functions.
#
#  Copyright (c) 2008, Rice University.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are
#  met:
#
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
#  * Neither the name of Rice University (RICE) nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
#  This software is provided by RICE and contributors "as is" and any
#  express or implied warranties, including, but not limited to, the
#  implied warranties of merchantability and fitness for a particular
#  purpose are disclaimed. In no event shall RICE or contributors be
#  liable for any direct, indirect, incidental, special, exemplary, or
#  consequential damages (including, but not limited to, procurement of
#  substitute goods or services; loss of use, data, or profits; or
#  business interruption) however caused and on any theory of liability,
#  whether in contract, strict liability, or tort (including negligence
#  or otherwise) arising in any way out of the use of this software, even
#  if advised of the possibility of such damage.
#
#  $Id$
#
#  Define pre and post helper functions for configure, build and
#  install scripts, and source the top-level config vars.
#
#  Note: we want this to work when called via "make config" and
#  through ./configure directly.  Thus, it's important to manage
#  cookies here too.
#

FETCH_COOKIE=.fetch-done
CONFIG_COOKIE=.config-done
BUILD_COOKIE=.build-done
INSTALL_COOKIE=.install-done
NOBUILD_COOKIE=.dont-build
OPTIONS_FILE=../options.conf

ORIG_PKG_DIR=`pwd`

die()
{
    echo "===> ($PKG_NAME) $0: $*"
    exit 1
}

pre_fetch()
{
    rm -f "$FETCH_COOKIE" "$CONFIG_COOKIE"
    rm -f "$BUILD_COOKIE" "$INSTALL_COOKIE"
}

post_fetch()
{
    cd "$ORIG_PKG_DIR" || die "unable to cd: $ORIG_PKG_DIR"
    touch "$FETCH_COOKIE"
}

pre_configure()
{
    rm -f "$CONFIG_COOKIE" "$BUILD_COOKIE" "$INSTALL_COOKIE"
    $MAKE fetch || die "make fetch failed"
    $MAKE prereqs || die "make prereqs failed"
}

post_configure()
{
    cd "$ORIG_PKG_DIR" || die "unable to cd: $ORIG_PKG_DIR"
    touch "$CONFIG_COOKIE"
}

pre_build()
{
    rm -f "$BUILD_COOKIE" "$INSTALL_COOKIE"
    $MAKE config || die "make config failed"
}

post_build()
{
    cd "$ORIG_PKG_DIR" || die "unable to cd: $ORIG_PKG_DIR"
    touch "$BUILD_COOKIE"
}

pre_install()
{
    rm -f "$INSTALL_COOKIE"
    $MAKE build || die "make build failed"
}

post_install()
{
    cd "$ORIG_PKG_DIR" || die "unable to cd: $ORIG_PKG_DIR"
    touch "$INSTALL_COOKIE"
}

make_work_dir()
{
    work="${1:-work}"
    test -d "$work" && rm -rf "$work"
    mkdir -p "$work"
}

#
# Respect the no-build cookie, if set.
#
if test -f "$NOBUILD_COOKIE" ; then
    echo "${PKG_NAME}: `cat ${NOBUILD_COOKIE}`"
    exit 0
fi

#
# Source the top-level config vars.
#
test -f "$OPTIONS_FILE" || die "must run top-level configure first"
. "$OPTIONS_FILE"

#
# MAKE is either exported from Makefile.inc, or else passed from
# top-level configure via options.conf.
#
if test "x$MAKE" = x ; then
    MAKE="$gnu_make"
fi
