#
# vcStripStd.pl
#
# Note: please don't convert this Perl script to a /bin/sh script
# that execs Perl unless we are now requiring Windows users to
# have a /bin/sh shell to do a build.
#
# Removes "std::" namespace specifiers from the standard input.
# Writes the result on the standard output.
#
# We use this to post-process the output from bison on Windows
# because bison 1.30 and newer include std:: specifies on
# some symbols (e.g., size_t) when compiled by a C++ compiler.
# Unfortunately, known versions of Visual C++ (e.g., 5 and 6)
# and the Platform SDK do not put these symbols in the std namespace.
#
# We could have used sed within the makefile to do the same thing,
# but we currently do not require sed to do a Windows build.  (We
# require Perl for tcl2c.)
#

while( <STDIN> )
{
	if( /(^.*#[ 	]*define[ 	]*.*)std::(.*)$/ )
	{
		# the line defines a symbol to be std-scoped
		# dump the line, replacing the std::x with x
		print "$1 $2\n";
	}
	else
	{
		# dump the line as it is
		print $_;
	}
}


