#!/usr/bin/perl
#
# irixHeaderCmp.pl
#
# Takes two arguments: An IRIX header file, and a revision number.
# The given header file will be parsed and compaired to the given
# revision number.
# 
# The script will print EQUAL, OLDER, NEWER, or ERROR, which should
# be interpreted in relation to the user specified revision # number.
# For example, file is OLDER than revision.

if ($#ARGV != 1 || ! -f $ARGV[0]) {
    print STDERR "Useage: irixHeaderCmp.pl <Header File> <Revision #>\n";
    exit;
}

my $filename = shift;
my $testRevision = shift;

# Open file and retrieve header revision.
open(HEADER, $filename) or print "ERROR" and exit;
while (<HEADER>) { last if (/\$Revision: [.\d]+ /); }
close(HEADER);

# Exit if we can't find the revision string.
print "ERROR" and exit if (! /\$Revision: ([.\d]+) /);
my $fileRevision = $1;

# Perform compairison.
my @a = split(/\./, $fileRevision);
my @b = split(/\./, $testRevision);

for (my $i = 0; defined($a[$i]) || defined($b[$i]); $i++) {
    print "OLDER" and exit if ($a[$i] < $b[$i]);
    print "NEWER" and exit if ($a[$i] > $b[$i]);
}
print "EQUAL"
