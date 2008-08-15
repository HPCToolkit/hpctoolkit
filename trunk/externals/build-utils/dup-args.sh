#
#  Remove duplicate or conflicting var=val arguments from the command
#  line, where the last one wins.
#
#  This needs to be a separate script and sourced, it won't work as a
#  function.
#
#  $Id$
#
mark='x--x--x'

set -- "$@" "$mark"

while test "x$1" != "x$mark"
do
    arg="$1"
    shift
    case "$arg" in
	*=* )
	    var=`expr "x$arg" : 'x\(.*\)=.*'`
	    ans=yes
	    for arg2 in "$@"
	    do
		if test "x$arg2" = "x$mark" ; then
		    break
		fi
		var2=`expr "x$arg2" : 'x\(.*\)=.*'`
		if test "x$var" = "x$var2" ; then
		    ans=no
		    break
		fi
	    done
	    if test "$ans" = yes ; then
		set -- "$@" "$arg"
	    fi
	    ;;

	* )
	    set -- "$@" "$arg"
	    ;;
    esac
done
shift
