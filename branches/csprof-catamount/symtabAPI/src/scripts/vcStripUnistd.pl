#
# vcStripUnistd.pl
#
# Note: please don't convert this Perl script to a /bin/sh script
# that execs Perl unless we are now requiring Windows users to
# have a /bin/sh shell to do a build.)
#
# Removes "#include <unistd.h>" line from the standard input
# and writes the result to standard output.
#
# We use this to post-process the output from flex on Windows
# because some popular versions of flex (e.g., from cygwin)
# contain this #include and there is no unistd.h available
# when compiling with Visual C++.
#
# We could have used sed within the makefile to do the same thing,
# but we currently do not require sed to do a Windows build.  (We
# require Perl for other purposes, namely tcl2c.)
#

while( <STDIN> )
{
	if( ! /^.*#[ 	]*include[ 	]*<unistd.h>.*$/ )
	{
		# dump the line as it is
		print $_;
	}
}


