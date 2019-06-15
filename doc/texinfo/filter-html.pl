#!/usr/bin/env perl
#
#  Read the raw html from texinfo on stdin and print the modified html
#  for hpctoolkit on stdout.
#
#  ./filter-html.pl  <html-from-texinfo  >html-for-hpctoolkit
#

open(PART1, "<part1") || die "unable to open: part1";

open(HEADER, "<../www/style/header-hpctoolkit.html")
    || die "unable to open: header-hpctoolkit.html";

open(FOOTER, "<../www/style/footer-hpctoolkit.html")
    || die "unable to open: footer-hpctoolkit.html";

#------------------------------------------------------------

# 1 -- <?xml>, <!DOCTYPE>, <head> from part1 file.

while ($_ = <PART1>) {
    print $_;
}

#------------------------------------------------------------

# 2 -- <title> from texinfo (stdin), skip lines before that.

while ($_ = <>) {
    if (m/<title/) {
	print "  $_";
	last;
    }
}

#------------------------------------------------------------

# 3 -- end </head>, begin <body>

print "</head>\n\n<body>\n\n";

#------------------------------------------------------------

# 4 -- hpctoolkit header

while ($_ = <HEADER>) {
    print $_;
}
print "\n";

#------------------------------------------------------------

# 5 -- main body from texinfo, starting with <h1>, up to </body>.

while ($_ = <>) {
    if (m/<body/) {
	last;
    }
}

while ($_ = <>) {
    if (m/<h/) {
	print $_;
	last;
    }
}

while ($_ = <>) {
    if (m|</body|) {
	last;
    }
    print $_;
}

#------------------------------------------------------------

# 6 -- hpctoolkit footer

while ($_ = <FOOTER>) {
    print $_;
}

#------------------------------------------------------------

# 7 -- end </body> and </html>

print "\n</body>\n</html>\n";

