#!/usr/bin/perl

# manServer - Unix man page to HTML converter
# Rolf Howarth, rolf@squarebox.co.uk
# Version 1.07  16 July 2001

$version = "1.07";
$manServerUrl = "<A HREF=\"http://www.squarebox.co.uk/download/manServer.shtml\">manServer $version</A>";

use Socket;

$ENV{'PATH'} = "/bin:/usr/bin";

initialise();
$request = shift @ARGV;
# Usage: manServer [-dn] filename | manServer [-s port]

$root = "";
$cgiMode = 0;
$bodyTag = "BODY bgcolor=#F0F0F0 text=#000000 link=#0000ff vlink=#C000C0 alink=#ff0000";

if ($ENV{'GATEWAY_INTERFACE'} ne "")
{
	*OUT = *STDOUT;
	open(LOG, ">>/tmp/manServer.log");
	chmod(0666, '/tmp/manServer.log');
	$root = $ENV{'SCRIPT_NAME'};
	$url = $ENV{'PATH_INFO'};
	if ($ENV{'REQUEST_METHOD'} eq "POST")
		{ $args = <STDIN>; chop $args; }
	else
		{ $args = $ENV{'QUERY_STRING'}; }
	$url .= "?".$args if ($args);
	$cgiMode = 1;
	$date = &fmtTime(time);
	$remoteHost = $ENV{'REMOTE_HOST'} || $ENV{'REMOTE_ADDR'};
	$referer = $ENV{'HTTP_REFERER'};
	$userAgent = $ENV{'HTTP_USER_AGENT'};
	print LOG "$date\t$remoteHost\t$url\t$referer\t$userAgent\n";
	processRequest($url);
}
elsif ($request eq "-s" || $request eq "")
{
	*LOG = *STDERR;
	startServer();
}
else
{
	$cmdLineMode = 1;
	if ($request =~ m/^-d(\d)/)
	{
		$debug = $1;
		$request = shift @ARGV;
	}
	*OUT = *STDOUT;
	*LOG = *STDERR;
	$file = findPage($request);
	man2html($file);
}

exit(0);


##### Mini HTTP Server ####

sub startServer
{
	($port) = @ARGV;
	$port = 8888 unless $port;

	$sockaddr = 'S n a4 x8';

	($name, $aliases, $proto) = getprotobyname('tcp');
	($name, $aliases, $port) = getservbyname($port, 'tcp')
			unless $port =~ /^\d+$/;

	while(1)
	{
		$this = pack($sockaddr, AF_INET, $port, "\0\0\0\0");

		select(NS); $| = 1; select(stdout);

		socket(S, PF_INET, SOCK_STREAM, $proto) || die "socket: $!";
		if (bind(S, $this))
		{
			last;
		}
		else
		{
			print STDERR "Failed to bind to port $port: $!\n";
			++$port;
		}
	}

	listen(S, 5) || die "connect: $!";

	select(S); $| = 1; select(stdout);

	while(1)
	{
		print LOG "Waiting for connection on port $port\n";
		($addr = accept(NS,S)) || die $!;
		#print "accept ok\n";

		($af,$rport,$inetaddr) = unpack($sockaddr,$addr);
		@inetaddr = unpack('C4',$inetaddr);
		print LOG "Got connection from ", join(".",@inetaddr), "\n";

		while (<NS>)
		{
			if (m/^GET (\S+)/) { $url = $1; }
			last if (m/^\s*$/);
		}
		*OUT = *NS;
		processRequest($url);
		close NS ;
	}
}


sub processRequest
{
	$url = $_[0];
	print LOG "Request = $url, root = $root\n";

	if ( ($url =~ m/^([^?]*)\?(.*)$/) || ($url =~ m/^([^&]*)&(.*)$/) )
	{
		$request = $1;
		$args = $2;
	}
	else
	{
		$request = $url;
		$args = "";
	}

	@params = split(/[=&]/, $args);
	for ($i=0; $i<=$#params; ++$i)
	{
		$params[$i] =~ tr/+/ /;
		$params[$i] =~ s/%([0-9A-Fa-f][0-9A-Fa-f])/pack("C",hex($1))/eg;
	}
	%params = @params;

	$request = $params{'q'} if ($params{'q'});
	$searchType = $params{'t'};
	$debug = $params{'d'};

	$processed = 0;
	$file = "";

	if ($searchType)
	{
		print OUT "HTTP/1.0 200 Ok\n" unless ($cgiMode);
		print OUT "Content-type: text/html\n\n";
		print OUT "<H1>Searching not yet implemented</H1>\n";
		print LOG "Searching not implemented\n";
		$processed = 1;
	}
	elsif ($request eq "/" || $request eq "")
	{
		print OUT "HTTP/1.0 200 Ok\n" unless ($cgiMode);
		print OUT "Content-type: text/html\n\n";
		print LOG "Home page\n";
		homePage();
		$processed = 1;
	}
	elsif ($request =~ m,^/.*/$,)
	{
		print OUT "HTTP/1.0 200 Ok\n" unless ($cgiMode);
		print OUT "Content-type: text/html\n\n";
		print LOG "List directory\n";
		listDir($request);
		$processed = 1;
	}
	elsif (-f $request || -f "$request.gz" || -f "$request.bz2")
	{
		# Only allow fully specified files if they're in our manpath
		foreach $md (@manpath)
		{
			$dir = $md;
			if (substr($request,0,length($dir)) eq $dir)
			{
				print OUT "HTTP/1.0 200 Ok\n" unless ($cgiMode);
				print OUT "Content-type: text/html\n\n";
				man2html($request);
				$processed = 1;
				last;
			}
		}
	}
	else
	{
		$file = findPage($request);
		if (@multipleMatches)
		{
			print OUT "HTTP/1.0 200 Ok\n" unless ($cgiMode);
			print OUT "Content-type: text/html\n\n";
			print LOG "Multiple matches\n";
			printMatches();
			$processed = 1;
		}
		elsif ($file)
		{
			print OUT "HTTP/1.0 301 Redirected\n" unless ($cgiMode);
			$file .= "&d=$debug" if ($debug);
			print OUT "Location: $root$file\n\n";
			print LOG "Redirect to $root$file\n";
			$processed = 1;
		}
	}

	unless ($processed)
	{
		print OUT "HTTP/1.0 404 Not Found\n" unless ($cgiMode);
		print OUT "Content-type: text/html\n\n";
		print OUT "<HTML><HEAD>\n<TITLE>Not Found</TITLE>\n<$bodyTag>\n";
		print OUT "<CENTER><H1><HR>Not Found<HR></H1></CENTER>\nFailed to find man page /$request\n";
		print OUT "<P><HR><P><A HREF=\"$root/\">Main Index</A>\n</HTML>\n";
		print STDERR "Failed to find /$request\n" unless ($cgiMode);
	}
}

sub homePage
{
	print OUT "<HTML><HEAD><TITLE>Manual Pages - Main Index</TITLE>
</HEAD><$bodyTag><CENTER><H1><HR><I>Manual Reference Pages</I> - Main Index<HR></H1></CENTER>
<FORM ACTION=\"$root/\" METHOD=get>\n";
	$uname = `uname -s -r`;
	if (! $?)
	{
		$hostname = `hostname`;
		print OUT "<B>$uname pages on $hostname</B><P>\n";
	}
	# print OUT "<SELECT name=t> <OPTION selected value=0>Command name
	# <OPTION value=1>Keyword search <OPTION value=2>Full text search</SELECT>\n";
	print OUT "Command name: <INPUT name=q size=20> <INPUT type=submit value=\"Show Page\"> </FORM><P>\n";
	loadManDirs();
	foreach $dir (@mandirs)
	{
		($section) = ($dir =~ m/man([0-9A-Za-z]+)$/);
		print OUT "<A HREF=\"$root$dir/\">$dir" ;
		print OUT "- <I>$sectionName{$section}</I>" if ($sectionName{$section});
		print OUT "</A><BR>\n";
	}
	print OUT "<P><HR><P><FONT SIZE=-1>Generated by $manServerUrl from local unix man pages.</FONT>\n</BODY></HTML>\n";
}

sub listDir
{
	foreach $md (@manpath)
	{
		$dir = $md;
		if (substr($request,0,length($dir)) eq $dir)
		{
			$request =~ s,/$,,;
			($section) = ($request =~ m/man([0-9A-Za-z]+)$/);
			$sectionName = $sectionName{$section};
			$sectionName = "Manual Reference Pages" unless ($sectionName);
			print OUT "<HTML><HEAD><TITLE>Contents of $request</TITLE></HEAD>\n<$bodyTag>\n";
			print OUT "<CENTER><H1><HR><NOBR><I>$sectionName</I></NOBR> - <NOBR>Index of $request</NOBR><HR></H1></CENTER>\n";
			print OUT "<FORM ACTION=\"$root/\" METHOD=get>\n";
			print OUT "Command name: <INPUT name=q size=20> <INPUT type=submit value=\"Show Page\"> </FORM><P>\n";

			if (opendir(DIR, $request))
			{
				@files = sort readdir DIR;
				foreach $f (@files)
				{
					next if ($f eq "." || $f eq ".." || $f !~ m/\./);
					$f =~ s/\.(gz|bz2)$//;
					# ($name) = ($f =~ m,/([^/]*)$,);
					print OUT "<A HREF=\"$root$request/$f\">$f</A>&nbsp;\n";
				}
				closedir DIR;
			}
			print OUT "<P><A HREF=\"$root/\">Main Index</A>\n</HTML>\n";
			print OUT "<P><HR><P><FONT SIZE=-1>Generated by $manServerUrl from local unix man pages.</FONT>\n</BODY></HTML>\n";
			return;
		}
	}
	print OUT "<H1>Directory $request not known</H1>\n";
}

sub printMatches
{
	print OUT "<HTML><HEAD><TITLE>Ambiguous Request '$request'</TITLE></HEAD>\n<$bodyTag>\n";
	print OUT "<CENTER><H1><HR>Ambiguous Request '$request'<HR></H1></CENTER>\nPlease select one of the following pages:<P><BLOCKQUOTE>";
	foreach $f (@multipleMatches)
	{
		print OUT "<A HREF=\"$root$f\">$f</A><BR>\n";
	}
	print OUT "</BLOCKQUOTE><HR><P><A HREF=\"$root/\">Main Index</A>\n</HTML>\n";
}


##### Process troff input using man macros into HTML #####

sub man2html
{
	$file = $_[0];
	$srcfile = $file;
	$zfile = $file;
	if (! -f $file)
	{
		if (-f "$file.gz")
		{
			$zfile = "$file.gz";
			$zcat = "/usr/bin/zcat";
			$zcat = "/bin/zcat" unless (-x $zcat);
			$srcfile = "$zcat $zfile |";
			$srcfile =~ m/^(.*)$/;
			$srcfile = $1;	# untaint
		}
		elsif (-f "$file.bz2")
		{
			$zfile = "$file.bz2";
			$srcfile = "/usr/bin/bzcat $zfile |";
			$srcfile =~ m/^(.*)$/;
			$srcfile = $1;	# untaint
		}
	}
	print LOG "man2html $file\n";
	$foundNroffTag = 0;
	loadContents($file);
	unless (open(SRC, $srcfile))
	{
		print OUT "<H1>Failed to open $file</H1>\n";
		print STDERR "Failed to open $srcfile\n";
		return;
	}
	($dir,$page,$sect) = ($file =~ m,^(.*)/([^/]+)\.([^.]+)$,);
	$troffTable = 0;
	%macro = ();
	%renamedMacro = ();
	%deletedMacro = ();
	@indent = ();
	@tabstops = ();
	$indentLevel = 0;
	$prevailingIndent = 6;
	$trapLine = 0;
	$blockquote = 0;
	$noSpace = 0;
	$firstSection = 0;
	$eqnStart = "";
	$eqnEnd = "";
	$eqnMode = 0;
	%eqndefs = ();
	$defaultNm = "";
	$title = $file;
	$title = "Manual Page - $page($sect)" if ($page && $sect);

	$_ = getLine();
	if (m/^.so (man.*)$/)
	{
		# An .so include on the first line only is replaced by the referenced page.
		# (See elsewhere for processing of included sections that occur later in document.)
		man2html("$dir/../$1");
		return;
	}

	$perlPattern = "";
	if ($file =~ m/perl/)
	{
		&loadPerlPages();
		$perlPattern = join('|', grep($_ ne $page, keys %perlPages));
	}

	print OUT "<HTML><HEAD>\n<TITLE>$title</TITLE>\n<$bodyTag><A NAME=top></A>\n";

	if ($foundNroffTag)
	{
		do
		{
			preProcessLine();
			processLine();
		}
		while(getLine());
		endNoFill();
		endParagraph();
	}
	else
	{
		# Special case where input is not nroff at all but is preformatted text
		$sectionName = "Manual Reference Pages";
		$sectionNumber = $sect;
		$left = "Manual Page";
		$right = "Manual Page";
		$macroPackage = "(preformatted text)";
		$pageName = "$page($sect)";
		$saveCurrentLine = $_;
		outputPageHead();
		$_ = $saveCurrentLine;
		print OUT "<PRE>\n";
		do
		{
			print OUT $_;
		}
		while(getLine());
		print OUT "</PRE>\n";
	}
	outputPageFooter();
}

sub outputPageHead
{
	plainOutput( "<CENTER>\n" );
	outputLine( "<H1><HR><I>$sectionName &nbsp;-&nbsp;</I><NOBR>$pageName</NOBR><HR></H1>\n" );
	plainOutput( "</CENTER>\n" );
}

sub outputPageFooter
{
	if ($pageName)
	{
		unless ($cmdLineMode)
		{
			plainOutput( "<FORM ACTION=\"$root/\" METHOD=get>\n" );
			plainOutput( "Jump to page &nbsp;<INPUT name=q size=12>&nbsp; or go to <A HREF=#top>Top of page</A>&nbsp;|&nbsp;\n" );
			plainOutput( "<A HREF=\"$root$dir/\">Section $sectionNumber</A>&nbsp;|&nbsp;\n" );
			plainOutput( "<A HREF=\"$root/\">Main Index</A>.\n" );
			plainOutput( "<FORM>\n" );
		}
		endBlockquote();
		outputLine("<P><HR>\n<TABLE width=100%><TR> <TD width=33%><I>$left</I></TD> <TD width=33% align=center>$pageName</TD> <TD align=right width=33%><I>$right</I></TD> </TR></TABLE>");
	}
	plainOutput("<FONT SIZE=-1>Generated by $manServerUrl from $zfile $macroPackage.</FONT>\n</BODY></HTML>\n");
}

sub outputContents
{
	print OUT "<A name=contents></A><H3>CONTENTS</H3></A>\n";
	blockquote();
	for ($id=1; $id<=$#contents; ++$id)
	{
		$name = $contents[$id];
		$pre = "";
		$pre = "&nbsp; &nbsp; &nbsp;" if ($name =~ m/^ /);
		$pre .= "&nbsp; &nbsp; &nbsp;" if ($name =~ m/^  /);
		$name =~ s,^\s+,,;
		next if ($name eq "" || $name =~ m,^/,);
		unless ($name =~ m/[a-z]/)
		{
			$name = "\u\L$name";
			$name =~ s/ (.)/ \u\1/g;
		}
		outputLine("$pre<A HREF=#$id>$name</A><BR>\n");
	}
	endBlockquote();
}

# First pass to extract table of contents
sub loadContents
{
	@contents = ();
	%contents = ();
	# print STDERR "SRCFILE = $srcfile\n";
	open(SRC, $srcfile) || return;
	while (<SRC>)
	{
		preProcessLine();
		$foundNroffTag = $foundNroffTag || (m/^\.(\\\"|TH|so) /);
		if (m/^\.(S[HShs]) ([A-Z].*)\s*$/)
		{
			$foundNroffTag = 1;
			$c = $1;
			$t = $2;
			$t =~ s/"//g;
			$id = @contents;
			if ($c eq "SH" || $c eq "Sh")
			{
				push(@contents, $t);
			}
			elsif ($t =~ m/\\f/)
			{
				$t =~ s/\\f.//g;
				push(@contents, "  $t");
			}
			else
			{
				push(@contents, " $t");
			}
			$contents{"\U$t"} = $id;
		}
	}
	close SRC;
}

# Preprocess $_
sub preProcessLine
{
	# Remove spurious white space to canonicise the input
	chop;
	$origLine = $_;
	s, $,,g;
	s,^',.,;	# treat non breaking requests as if there was a dot
	s,^\.\s*,\.,;

	if ($eqnMode == 1)
	{
		if (m/$eqnEnd/)
		{
			s,^(.*?)$eqnEnd,&processEqnd($1),e;
			$eqnMode = 0;
		}
		else
		{
			&processEqns($_);
		}
	}
	if ($eqnStart && $eqnMode==0)
	{
		s,$eqnStart(.*?)$eqnEnd,&processEqnd($1),ge;
		if (m/$eqnStart/)
		{
			s,$eqnStart(.*)$,&processEqns($1),e;
			$eqnMode = 1;
		}
	}

	# XXX Note: multiple levels of escaping aren't handled properly, eg. \\*.. as a macro argument
	# should get interpolated as string but ends up with a literal '\' being copied through to output.
	s,\\\\\*q,&#34;,g; # treat mdoc \\*q as special case
	
	s,\\\\,_DBLSLASH_,g;
	s,\\ ,_SPACE_,g;
	s,\s*\\".*$,,;
	s,\\$,,;

	# Then apply any variable substitutions and escape < and >
	# (which has to be done before we start inserting tags...)
	s,\\\*\((..),$vars{$1},ge;
	s/\\\*([*'`,^,:~].)/$vars{$1}||"\\*$1"/ge;
	s,\\\*(.),$vars{$1},ge;
	# Expand special characters for the first time (eg. \(<-
	s,\\\((..),$special{$1}||"\\($1",ge;
	s,<,&lt;,g;
	s,>,&gt;,g;

	# Interpolate width and number registers
	s,\\w(.)(.*?)\1,&width($2),ge;
	s,\\n\((..),&numreg($1),ge;
	s,\\n(.),&numreg($1),ge;
}

# Undo slash escaping, normally done at output stage, also in macro defn
sub postProcessLine
{
	s,_DBLSLASH_,\\,g;
	s,_SPACE_, ,g;
}

# Rewrite the line, expanding escapes such as font styles, and output it.
# The line may be a plain text troff line, or it might be the expanded output of a
# macro in which case some HTML tags may already have been inserted into the text.
sub outputLine
{
	$_ = $_[0];

	print OUT "<!-- Output: \"$_\" -->\n" if ($debug>1);

	if ($needBreak)
	{
		plainOutput("<!-- Need break --><BR>\n");
		lineBreak();
	}
	if ($textSinceBreak && !$noFill && $_ =~ m/^\s/)
	{
		plainOutput("<BR>\n");
		lineBreak();
	}

	s,\\&\.,&#46;,g;    # \&. often used to escape dot at start of line
	s,\\\.,&#46;,g;
	s,\\\^,,g;
	s,\\\|,,g;
	s,\\c,,g;
	s,\\0,&nbsp;,g;
	s,\\t,\t,g;

	s,\\%,&nbsp;,g;
	s,\\{,,g;
	s,\\},,g;
	s,\\$,,g;

	s,\\e,&#92;,g;
	s,\\([-+_~#[]),\1,g;

	# Can't implement local motion tags
	s,\\[hv](.).*?\1,,g;
	s,\\z,,g;

	# Font changes, super/sub-scripts and font size changes
	s,\\(f[^(]|f\(..|u|d|s[-+]?\d),&inlineStyle($1),ge;

	# Overstrike
	if (m/\\o/)
	{
		# handle a few special accent cases we know how to deal with
		s,\\o(.)([aouAOU])"\1,\\o\1\2:\1,g;
		s,\\o(.)(.)\\(.)\1,\\o\1\2\3\1,g;
		s;\\o(.)([A-Za-z])(['`:,^~])\1;\\o\1\3\2\1;g;
		#s,\\o(.)(.*?)\1,"<BLINK>".($vars{$2}||$2)."</BLINK>",ge;
		s,\\o(.)(.*?)\1,$vars{$2}||$2,ge;
	}
	# Bracket building (ignore)
	s,\\b(.)(.*?)\1,\2,g;

	s,\\`,&#96;,g;
	s,\\',&#39;,g;
	s,',&#146;,g;
	s,`,&#145;,g;

	# Expand special characters introduced by eqn
	s,\\\((..),$special{$1}||"\\($1",ge;
	s,\\\((..),<BLINK>\\($1</BLINK>,g unless (m,^\.,);

	# Don't know how to handle other escapes
	s,(\\[^&]),<BLINK>\1</BLINK>,g unless (m,^\.,);

	postProcessLine();

	# Insert links for http, ftp and mailto URLs
	# Recognised URLs are sequence of alphanumerics and special chars like / and ~
	# but must finish with an alphanumeric rather than punctuation like "."
	s,\b(http://[-\w/~:@.%#+$?=]+\w),<A HREF=\"\1\">\1</A>,g;
	s,\b(ftp://[-\w/~:@.%#+$?=]+),<A HREF=\"\1\">\1</A>,g;
	s,([-_A-Za-z0-9.]+@[A-Za-z][-_A-Za-z0-9]*\.[-_A-Za-z0-9.]+),<A HREF=\"mailto:\1\">\1</A>,g;

	# special case for things like 'perlre' as it's so useful but the
	# pod-generated pages aren't very parser friendly...
	if ($perlPattern && ! m/<A HREF/i)
	{
		s,\b($perlPattern)\b,<A HREF=\"$root$perlPages{$1}\">\1</A>,g;
	}

	# Do this late so \& can be used to suppress conversion of URLs etc.
	s,\\&,,g;

	# replace tabs with spaces to next multiple of 8
	if (m/\t/)
	{
		$tmp = $_;
		$tmp =~ s/<[^>]*>//g;
		$tmp =~ s/&[^;]*;/@/g;
		@tmp = split(/\t/, $tmp);
		$pos = 0;
		for ($i=0; $i<=$#tmp; ++$i)
		{
			$pos += length($tmp[$i]);
			$tab[$i] = 0;
			$tab[$i] = 8 - $pos%8 unless (@tabstops);
			foreach $ts (@tabstops)
			{
				if ($pos < $ts)
				{
					$tab[$i] = $ts-$pos;
					last;
				}
			}
			$pos += $tab[$i];
		}
		while (m/\t/)
		{
			s,\t,"&nbsp;" x (shift @tab),e;
		}
	}

	$textSinceBreak = $_ unless ($textSinceBreak);
	print OUT $_;
}

# Output a line consisting purely of HTML tags which shouldn't be regarded as
# a troff output line.
sub plainOutput
{
	print OUT $_[0];
}


# Output the original line for debugging
sub outputOrigLine
{
	print OUT "<!-- $origLine -->\n";
}

# Use this to read the next input line (buffered to implement lookahead)
sub getLine
{
	$lookaheadPtr = 0;
	if (@lookahead)
	{
		$_ =  shift @lookahead;
		return $_;
	}
	$_ = <SRC>;
}

# Look ahead to peek at the next input line
sub _lookahead
{
	# set lookaheadPtr to 0 to re-read the lines we've looked ahead at
	if ($lookaheadPtr>=0 && $lookaheadPtr <= $#lookahead)
	{
		return $lookahead[$lookaheadPtr++];
	}
	$lookaheadPtr = -1;
	$ll = <SRC>;
	push(@lookahead, $ll);
	return $ll;
}

# Consume the last line that was returned by lookahead
sub consume
{
	--$lookaheadPtr;
	if ($lookaheadPtr>=0 && $lookaheadPtr <= $#lookahead)
	{
		$removed = $lookahead[$lookaheadPtr];
		@lookahead = (@lookahead[0..$lookaheadPtr-1],@lookahead[$lookaheadPtr+1..$#lookahead]);
	}
	else
	{
		$removed = pop @lookahead;
	}
	chop $removed;
	plainOutput("<!-- Consumed $removed -->\n");
}

# Look ahead skipping comments and other common non-text tags
sub lookahead
{
	$ll = _lookahead();
	while ($ll =~ m/^\.(\\"|PD|IX|ns)/)
	{
		$ll = _lookahead();
	}
	return $ll;
}

# Process $_, expaning any macros into HTML and calling outputLine().
# If necessary, this method can read more lines of input from <SRC> (.ig & .de)
# The following state variables are used:
# ...
sub processLine
{
	$doneLine = 1;	# By default, this counts as a line for trap purposes

	s,^\.if t ,,;
	s,^\.el ,,;		# conditions assumed to evaluate false, so else must be true...

	if ($troffTable)
	{
		processTable();
	}
	elsif ($eqnMode == 2)	
	{
		plainOutput("<!-- $_ -->\n");
		processEqns($_);
	}
	elsif (m/^\./)
	{
		processMacro();
	}
	else
	{
		processPlainText();
	}
	if ($doneLine)
	{
		# Called after processing (most) input lines to decrement trapLine. This is needed
		# to implement the .it 1 trap after one line for .TP, where the first line is outdented
		if ($trapLine > 0)
		{
			--$trapLine;
			if ($trapLine == 0)
			{
				&$trapAction;
			}
		}
	}
}


# Process plain text lines
sub processPlainText
{
	if ($_ eq "")
	{
		lineBreak();
		plainOutput("<P>\n");
		return;
	}

	s,(\\f[23BI])([A-Z].*?)(\\f.),$1.($contents{"\U$2"}?"<A HREF=#".$contents{"\U$2"}.">$2</A>":$2).$3,ge;

	if ($currentSection eq "SEE ALSO" && ! $cmdLineMode)
	{
		# Some people don't use BR or IR for see also refs
		s,(^|\s)([-.A-Za-z_0-9]+)\s?\(([0-9lL][0-9a-zA-Z]*)\),\1<A HREF=\"$root/$2.$3\">$2($3)</A>,g;
	}
	outputLine("$_\n");
}


# Process macros and built-in directives
sub processMacro
{
	outputOrigLine();

	# Place macro arguments (space delimited unless within ") into @p
	# Remove " from $_, place command in $c, remainder in $joined

	@p = grep($_ !~ m/^\s*$/, split(/("[^"]*"|\s+)/) );
	grep(s/"//g, @p);
	$_ = join(" ", @p);
	$p[0] =~ s/^\.//;
	$c = $p[0];
	$joined = join(" ", @p[1..$#p]);
	$joined2 = join(" ", @p[2..$#p]);
	$joined3 = join(" ", @p[3..$#p]);

	if ($macro{$c})				# Expand macro
	{
		# Get full macro text
		$macro = $macro{$c};
		# Interpolate arguments
		$macro =~ s,\\\$(\d),$p[$1],ge;
		#print OUT "<!-- Expanding $c to\n$macro-->\n";
		foreach $_ (split(/\n/, $macro))
		{
			$_ .= "\n";
			preProcessLine();
			processLine();
		}
		$doneLine = 0;
		return;
	}
	elsif ($renamedMacro{$c})
	{
		$c = $renamedMacro{$c};
	}

	if ($c eq "ds")			# Define string
	{
		$vars{$p[1]} = $joined2;
		$doneLine = 0;
	}
	elsif ($c eq "nr")			# Define number register
	{
		$number{$p[1]} = evalnum($joined2);
		$doneLine = 0;
	}
	elsif ($c eq "ti")			# Temporary indent
	{
		plainOutput("&nbsp; &nbsp;");
	}
	elsif ($c eq "rm")
	{
		$macroName = $p[1];
		if ($macro{$macroName})
		{
			delete $macro{$macroName};
		}
		else
		{
			$deletedMacro{$macroName} = 1;
		}
	}
	elsif ($c eq "rn")
	{
		$oldName = $p[1];
		$newName = $p[2];
		$macro = $macro{$oldName};
		if ($macro)
		{
			if ($newName =~ $reservedMacros && ! $deletedMacro{$newName})
			{
				plainOutput("<!-- Not overwriting reserved macro '$newName' -->\n");
			}
			else
			{
				$macro{$newName} = $macro;
				delete $deletedMacro{$newName};
			}
			delete $macro{$oldName};
		}
		else
		{
			# Support renaming of reserved macros by mapping occurrences of new name
			# to old name after macro expansion so that built in definition is still
			# available, also mark the name as deleted to override reservedMacro checks.
			plainOutput("<!-- Fake renaming reserved macro '$oldName' -->\n");
			$renamedMacro{$newName} = $oldName;
			$deletedMacro{$oldName} = 1;
		}
	}
	elsif ($c eq "de" || $c eq "ig")	# Define macro or ignore
	{
		$macroName = $p[1];
		if ($c eq "ig")
			{ $delim = ".$p[1]"; }
		else
			{ $delim = ".$p[2]"; }
		$delim = ".." if ($delim eq ".");
		# plainOutput("<!-- Scanning for delimiter $delim -->\n");

		$macro = "";
		$_ = getLine();
		preProcessLine();
		while ($_ ne $delim)
		{
			postProcessLine();
			outputOrigLine();
			$macro .= "$_\n";
			$_ = getLine();
			last if ($_ eq "");
			preProcessLine();
		}
		outputOrigLine();
		# plainOutput("<!-- Found delimiter -->\n");
		if ($c eq "de")
		{
			if ($macroName =~ $reservedMacros && ! $deletedMacro{$macroName})
			{
				plainOutput("<!-- Not defining reserved macro '$macroName' ! -->\n");
			}
			else
			{
				$macro{$macroName} = $macro;
				delete $deletedMacro{$macroName};
			}
		}
	}
	elsif ($c eq "so")			# Source
	{
		plainOutput("<P>[<A HREF=\"$root$dir/../$p[1]\">Include document $p[1]</A>]<P>\n");
	}
	elsif ($c eq "TH" || $c eq "Dt")			# Man page title
	{
		endParagraph();
		$sectionNumber = $p[2];
		$sectionName = $sectionName{"\L$sectionNumber"};
		$sectionName = "Manual Reference Pages" unless ($sectionName);
		$pageName = "$p[1] ($sectionNumber)";
		outputPageHead();
		if ($c eq "TH")
		{
			$right = $p[3];
			$left = $p[4];
			$left = $osver unless ($left);
			$macroPackage = "using man macros";
		}
		else
		{
			$macroPackage = "using doc macros";
		}
	}
	elsif ($c eq "Nd")
	{
		outputLine("- $joined\n");
	}
	elsif ($c eq "SH" || $c eq "SS" || $c eq "Sh" || $c eq "Ss")		# Section/subsection
	{
		lineBreak();
		endNoFill();
		endParagraph();
		$id = $contents{"\U$joined"};
		$currentSection = $joined;

		if ($c eq "SH" || $c eq "Sh")
		{
			endBlockquote();
			if ($firstSection++==1)	# after first 'Name' section
			{
				outputContents();
			}
			outputLine( "<A name=$id>\n\n     <H3>$joined</H3>\n\n</A>\n" );
			blockquote();
		}
		elsif ($joined =~ m/\\f/)
		{
			$joined =~ s/\\f.//g;
			$id = $contents{"\U$joined"};
			outputLine( "<A name=$id>\n<H4><I>$joined</I></H4></A>\n" );
		}
		else
		{
			endBlockquote();
			outputLine( "<A name=$id>\n\n    <H4>&nbsp; &nbsp; $joined</H4>\n</A>\n" );
			blockquote();
		}
		lineBreak();
	}
	elsif ($c eq "TX" || $c eq "TZ")	# Document reference
	{
		$title = $title{$p[1]};
		$title = "Document [$p[1]]" unless ($title);
		outputLine( "\\fI$title\\fP$joined2\n" );
	}
	elsif ($c eq "PD")			# Line spacing
	{
		$noSpace = ($p[1] eq "0");
		$doneLine = 0;
	}
	elsif ($c eq "TS")			# Table start
	{
		unless ($macroPackage =~ /tbl/)
		{
			if ($macroPackage =~ /eqn/)
				{ $macroPackage =~ s/eqn/eqn & tbl/; }
			else
				{ $macroPackage .= " with tbl support"; }
		}
		resetStyles();
		endNoFill();
		$troffTable = 1;
		$troffSeparator = "\t";
		plainOutput( "<P><BLOCKQUOTE><TABLE bgcolor=#E0E0E0 border=1 cellspacing=0 cellpadding=3>\n" );
	}
	elsif ($c eq "EQ")			# Eqn start
	{
		unless ($macroPackage =~ /eqn/)
		{
			if ($macroPackage =~ /tbl/)
				{ $macroPackage =~ s/tbl/tbl & eqn/; }
			else
				{ $macroPackage .= " with eqn support"; }
		}
		$eqnMode = 2;
	}
	elsif ($c eq "ps")			# Point size
	{
		plainOutput(&sizeChange($p[1]));
	}
	elsif ($c eq "ft")			# Font change
	{
		plainOutput(&fontChange($p[1]));
	}
	elsif ($c eq "I" || $c eq "B")	# Single word font change
	{
		$id = $contents{"\U$joined"};
		if ($id && $joined =~ m/^[A-Z]/)
			{ $joined = "<A HREF=#$id>$joined</A>"; }
		outputLine( "\\f$c$joined\\fP " );
		plainOutput("\n") if ($noFill);
	}
	elsif ($c eq "SM")			# Single word smaller
	{
		outputLine("\\s-1$joined\\s0 ");
		$doneLine = 0 unless ($joined);
	}
	elsif ($c eq "SB")			# Single word bold and small
	{
		outputLine("\\fB\\s-1$joined\\s0\\fP ");
	}
	elsif (m/^\.[BI]R (\S+)\s?\(\s?([0-9lL][0-9a-zA-Z]*)\s?\)(.*)$/)
	{
		# Special form, .BR is generally used for references to other pages
		# Annoyingly, some people have more than one per line...
		# Also, some people use .IR ...
		for ($i=1; $i<=$#p; $i+=2)
		{
			$pair = $p[$i]." ".$p[$i+1];
			if ($p[$i+1] eq "(")
			{
				$pair .= $p[$i+2].$p[$i+3];
				$i += 2;
			}
			if ($pair =~ m/^(\S+)\s?\(\s?([0-9lL][0-9a-zA-Z]*)\s?\)(.*)$/)
			{
				if ($cmdLineMode)
					{ outputLine( "\\fB$1\\fR($2)$3\n" ); }
				else
					{ outputLine( "<A HREF=\"$root/$1.$2\">$1($2)</A>$3\n" ); }
			}
			else
				{ outputLine( "$pair\n" ); }
		}
	}
	elsif ($c eq "BR" || $c eq "BI" || $c eq "IB" ||
		   $c eq "IR" || $c eq "RI" || $c eq "RB")
	{
		$f1 = (substr($c ,0,1));
		$f2 = (substr($c,1,1));

		# Check if first param happens to be a section name
		$id = $contents{"\U$p[1]"};
		if ($id && $p[1] =~ m/^[A-Z]/)
		{
			$p[1] = "<A HREF=#$id>$p[1]</A>";
		}

		for ($i=1; $i<=$#p; ++$i)
		{
			$f = ($i%2 == 1) ? $f1 : $f2;
			outputLine("\\f$f$p[$i]");
		}
		outputLine("\\fP ");
		plainOutput("\n") if ($noFill);
	}
	elsif ($c eq "nf" || $c eq "Bd")			# No fill
	{
		startNoFill();
	}
	elsif ($c eq "fi" || $c eq "Ed")			# Fill
	{
		endNoFill();
	}
	elsif ($c eq "HP")
	{
		$indent = evalnum($p[1]);
		if ($trapOnBreak)
		{
			plainOutput("<BR>\n");
		}
		else
		{
			# Outdent first line, ie. until next break
			$trapOnBreak = 1;
			$trapAction = *trapHP;
			newParagraph($indent);
			plainOutput( "<TD colspan=2>\n" );
			$colState = 2;
		}
	}
	elsif ($c eq "IP")
	{
		$trapOnBreak = 0;
		$tag = $p[1];
		$indent = evalnum($p[2]);
		newParagraph($indent);
		outputLine("<TD$width>\n$tag\n</TD><TD>\n");
		$colState = 1;
		lineBreak();
	}
	elsif ($c eq "TP")
	{
		$trapOnBreak = 0;
		$trapLine = 1;	# Next line is tag, then next column
		$doneLine = 0;	# (But don't count this line)
		$trapAction = *trapTP;
		$indent = evalnum($p[1]);
		$tag = lookahead();
		chop $tag;
		$i = ($indent ? $indent : $prevailingIndent) ;
		$w = width($tag);
		if ($w > $i)
		{
			plainOutput("<!-- Length of tag '$tag' ($w) > indent ($i) -->\n") if ($debug);
			newParagraph($indent);
			$trapAction = *trapHP;
			plainOutput( "<TD colspan=2>\n" );
			$colState = 2;
		}
		else
		{
			newParagraph($indent);
			plainOutput( "<TD$width nowrap>\n" );
			$colState = 0;
		}
		$body = lookahead();
		$lookaheadPtr = 0;
		if ($body =~ m/^\.[HILP]?P/)
		{
			chop $body;
			plainOutput("<!-- Suppressing TP body due to $body -->\n");
			$trapLine = 0;
		}
	}
	elsif ($c eq "LP" || $c eq "PP" || $c eq "P" || $c eq "Pp")	# Paragraph
	{
		$trapOnBreak = 0;
		$prevailingIndent = 6;
		if ($indent[$indentLevel] > 0 && $docListStyle eq "")
		{
			$line = lookahead();
			if ($line =~ m/^\.(TP|IP|HP)/)
			{
				plainOutput("<!-- suppressed $c before $1 -->\n");
			}
			elsif ($line =~ m/^\.RS/)
			{
				plainOutput("<P>\n");
			}
			else
			{
				endRow();
				$foundTag = "";
				$lookaheadPtr = 0;
				do
				{
					$line = lookahead();
					if ($line =~ m/^\.(TP|HP|IP|RS)( \d+)?/)
					{
						$indent = $2;
						$indent = $prevailingIndent unless ($2);
						if ($indent == $indent[$indentLevel])
							{ $foundTag = $1; }
						$line = "";
					}
				}
				while ($line ne "" && $line !~ m/^\.(RE|SH|SS|PD)/);
				$lookaheadPtr = 0;
				if ($foundTag)
				{
					plainOutput("<!-- Found tag $foundTag -->\n");
					plainOutput("<TR><TD colspan=2>\n");
					$colState = 2;
				}
				else
				{
					plainOutput("<!-- $c ends table -->\n");
					setIndent(0);
				}
			}
		}
		else
		{
			plainOutput("<P>\n");
		}
		lineBreak();
	}
	elsif ($c eq "br")			# Break
	{
		if ($trapOnBreak)
		{
			# Should this apply to all macros that cause a break?
			$trapOnBreak = 0;
			&$trapAction();
		}
		$needBreak = 1 if ($textSinceBreak);
	}
	elsif ($c eq "sp")			# Space
	{
		lineBreak();
		plainOutput("<P>\n");
	}
	elsif ($c eq "RS")			# Block indent start
	{
		if ($indentLevel==0 && $indent[0]==0)
		{
			blockquote();
		}
		else
		{
			$indent = $p[1];
			$indent = $prevailingIndent unless ($indent);
			if ($indent > $indent[$indentLevel] && !$extraIndent)
			{
				$extraIndent = 1;
				++$indentLevel;
				$indent[$indentLevel] = 0;
				setIndent($indent-$indent[$indentLevel-1]);
				plainOutput("<TR><TD$width>&nbsp;</TD><TD>\n");
				$colState = 1;
			}
			elsif ($indent < $indent[$indentLevel] || $colState==2)
			{
				endRow();
				setIndent($indent);
				plainOutput("<TR><TD$width>&nbsp;</TD><TD>\n");
				$colState = 1;
			}
			++$indentLevel;
			$indent[$indentLevel] = 0;
		}
		$prevailingIndent = 6;
	}
	elsif ($c eq "RE")			# Block indent end
	{
		if ($extraIndent)
		{
			endRow();
			setIndent(0);
			--$indentLevel;
			$extraIndent = 0;
		}
		if ($indentLevel==0)
		{
			endParagraph();
			if ($blockquote>0)
			{
				plainOutput("</BLOCKQUOTE>\n");
				--$blockquote;
			}
		}
		else
		{
			endRow();
			setIndent(0);
			--$indentLevel;
		}
		$prevailingIndent = $indent[$indentLevel];
		$prevailingIndent = 6 unless($prevailingIndent);
	}
	elsif ($c eq "DT")			# default tabs
	{
		@tabstops = ();
	}
	elsif ($c eq "ta")			# Tab stops
	{
		@tabstops = ();
		for ($i=0; $i<$#p; ++$i)
		{
			$ts = $p[$i+1];
			$tb = 0;
			if ($ts =~ m/^\+/)
			{
				$tb = $tabstops[$i-1];
				$ts =~ s/^\+//;
			}
			$ts = evalnum($ts);
			$tabstops[$i] = $tb + $ts;
		}
		plainOutput("<!-- Tabstops set at ".join(",", @tabstops)." -->\n") if ($debug);
	}
	elsif ($c eq "It")			# List item (mdoc)
	{
		lineBreak();
		if ($docListStyle eq "-tag")
		{
			endRow() unless($multilineIt);
			if ($tagWidth)
			{
				setIndent($tagWidth);
			}
			else
			{
				setIndent(6);
				$width = "";	# let table take care of own width
			}
			if ($p[1] eq "Xo")
			{
				plainOutput("<TR valign=top><TD colspan=2>");
			}
			else
			{
				$tag = &mdocStyle(@p[1..$#p]);
				$body = lookahead();
				if ($body =~ m/^\.It/)
					{ $multilineItNext = 1; }
				else
					{ $multilineItNext = 0; }
				if ($multilineIt)
				{
					outputLine("<BR>\n$tag\n");
				}
				elsif ($multilineItNext || $tagWidth>0 && width($tag)>$tagWidth)
				{
					outputLine("<TR valign=top><TD colspan=2>$tag\n");
					$colState = 2;
				}
				else
				{
					outputLine("<TR valign=top><TD>$tag\n");
					$colState = 1;
				}
				if ($multilineItNext)
				{
					$multilineIt = 1;
				}
				else
				{
					$multilineIt = 0;
					if ($colState==2)
						{ plainOutput("</TD></TR><TR><TD>&nbsp;</TD><TD>\n"); }
					else
						{ plainOutput("</TD><TD>\n"); }
				}
			}
		}
		else
		{
			plainOutput("<LI>");
		}
		lineBreak();
	}
	elsif ($c eq "Xc")
	{
		if ($docListStyle eq "-tag")
		{
			plainOutput("</TD></TR><TR><TD>&nbsp;</TD><TD>\n");
		}
	}
	elsif ($c eq "Bl")		# Begin list (mdoc)
	{
		push @docListStyles, $docListStyle;
		if ($p[1] eq "-enum")
		{
			plainOutput("<OL>\n");
			$docListStyle = $p[1];
		}
		elsif($p[1] eq "-bullet")
		{
			plainOutput("<UL>\n");
			$docListStyle = $p[1];
		}
		else
		{
			$docListStyle = "-tag";
			if ($p[2] eq "-width")
			{
				$tagWidth = width($p[3]);
				if ($tagWidth < 6) { $tagWidth = 6; }
			}
			else
			{
				$tagWidth = 0;
			}
			$multilineIt = 0;
		}
	}
	elsif ($c eq "El")		# End list
	{
		if ($docListStyle eq "-tag")
		{
			endRow();
			setIndent(0);
		}
		elsif ($docListStyle eq "-bullet")
		{
			plainOutput("</UL>\n");
		}
		else
		{
			plainOutput("</OL>\n");
		}
		$docListStyle = pop @docListStyles;
	}
	elsif ($c eq "Os")
	{
		$right = $joined;
	}
	elsif ($c eq "Dd")
	{
		$left = $joined;
	}
	elsif ($c eq "Sx")		# See section
	{
		$id = $contents{"\U$joined"};
		if ($id && $joined =~ m/^[A-Z]/)
		{
			outputLine("<A HREF=#$id>".&mdocStyle(@p[1..$#p])."</A>\n");
		}
		else
		{
			my $x = &mdocStyle(@p[1..$#p]);
			$x =~ s/^ //;
			outputLine($x."\n");
		}
	}
	elsif (&mdocCallable($c))
	{
		my $x = &mdocStyle(@p);
		$x =~ s/^ //;
		outputLine($x."\n");
	}
	elsif ($c eq "Bx")
	{
		outputLine("<I>BSD $joined</I>\n");
	}
	elsif ($c eq "Ux")
	{
		outputLine("<I>Unix $joined</I>\n");
	}
	elsif ($c eq "At")
	{
		outputLine("<I>AT&T $joined</I>\n");
	}
	elsif ($c =~ m/[A-Z][a-z]/)		# Unsupported doc directive
	{
		outputLine("<BR>.$c $joined\n");
	}
	elsif ($c eq "")				# Empty line (eg. troff comment)
	{
		$doneLine = 0;
	}
	else						# Unsupported directive
	{
		# Unknown macros are ignored, and don't count as a line as far as trapLine goes
		$doneLine = 0;
		plainOutput("<!-- ignored unsupported tag .$c -->\n");
	}
}

sub trapTP
{
	$lookaheadPtr = 0;
	$body = lookahead();
	if ($body =~ m/^\.TP/)
	{
		consume();
		$trapLine = 1;	# restore TP trap
		$doneLine = 0;	# don't count this line
		plainOutput("<BR>\n");
	}
	else
	{
		plainOutput("</TD><TD valign=bottom>\n");
		$colState = 1;
	}
	lineBreak();
}

sub trapHP
{
	$lookaheadPtr = 0;
	$body = lookahead();
	if ($body =~ m/^\.([TH]P)/)
	{
		consume();
		# Restore appropriate type of trap
		if ($1 eq "TP")
		{
			$trapLine = 1;
			$doneLine = 0;	# don't count this line
		}
		else
		{
			$trapOnBreak = 1;
		}
		plainOutput("<BR>\n");
	}
	else
	{
		plainOutput("</TD></TR><TR valign=top><TD$width>&nbsp;</TD><TD>\n");
		$colState = 1;
	}
	lineBreak();
}

sub newParagraph
{
	$indent = $_[0];
	endRow();
	startRow($indent);
}

sub startRow
{
	$indent = $_[0];
	$indent = $prevailingIndent unless ($indent);
	$prevailingIndent = $indent;
	setIndent($indent);
	plainOutput( "<TR valign=top>" );
}

# End an existing HP/TP/IP/RS row
sub endRow
{
	if ($indent[$indentLevel] > 0)
	{
		lineBreak();
		plainOutput( "</TD></TR>\n" );
	}
}

# Called when we output a line break tag. Only needs to be called once if
# calling plainOutput, but should call before and after if using outputLine.
sub lineBreak
{
	$needBreak = 0;
	$textSinceBreak = 0;
}

# Called to reset all indents and pending paragraphs (eg. at the start of
# a new top level section).
sub endParagraph
{
	++$indentLevel;
	while ($indentLevel > 0)
	{
		--$indentLevel;
		if ($indent[$indentLevel] > 0)
		{
			endRow();
			setIndent(0);
		}
	}
}

# Interpolate a number register (possibly autoincrementing)
sub numreg
{
	return 0 + $number{$_[0]};
}

# Evaluate a numeric expression
sub evalnum
{
	$n = $_[0];
	return "" if ($n eq "");
	if ($n =~ m/i$/)	# inches
	{
		$n =~ s/i//;
		$n *= 10;
	}
	return 0+$n;
}

sub setIndent
{
	$tsb = $textSinceBreak;
	$indent = evalnum($_[0]);
	if ($indent==0 && $_[0] !~ m/^0/)
	{
		$indent = 6;
	}
	plainOutput("<!-- setIndent $indent, indent[$indentLevel] = $indent[$indentLevel] -->\n") if ($debug);
	if ($indent[$indentLevel] != $indent)
	{
		lineBreak();
		if ($indent[$indentLevel] > 0)
		{
			plainOutput("<TR></TR>") unless ($noSpace);
			plainOutput("</TABLE>");
		}
		if ($indent > 0)
		{
			endNoFill();
			$border = "";
			$border = " border=1" if ($debug>2);
			#plainOutput("<P>") unless ($indent[$indentLevel] > 0);
			plainOutput("<TABLE$border");
			# Netscape bug, makes 2 cols same width? : plainOutput("<TABLE$border COLS=2");
			# Overcome some of the vagaries of Netscape tables
			plainOutput(" width=100%") if ($indentLevel>0);
			if ($noSpace)
			{
				plainOutput(" cellpadding=0 cellspacing=0>\n");
			}
			else
			{
				plainOutput(" cellpadding=3>".($tsb ? "<!-- tsb: $tsb -->\n<TR></TR><TR></TR>\n" : "\n") );
			}
			#$width = " width=".($indent*5);	# causes text to be chopped if too big
			$percent = $indent;
			if ($indentLevel > 0)
				{ $percent = $indent * 100 / (100-$indentLevel[0]); }
			$width = " width=$percent%";
		}
		$indent[$indentLevel] = $indent;
	}
}

# Process mdoc style macros recursively, as one of the macro arguments
# may itself be the name of another macro to invoke.
sub mdocStyle
{
	return "" unless @_;
	my ($tag, @param) = @_;
	my ($rest, $term);

	# Don't format trailing punctuation
	if ($param[$#param] =~ m/^[.,;:]$/)
	{
		$term = pop @param;
	}
	if ($param[$#param] =~ m/^[)\]]$/)
	{
		$term = (pop @param).$term;
	}

	if ($param[0] =~ m,\\\\,)
	{
		print STDERR "$tag: ",join(",", @param),"\n";
	}
	$rest = &mdocStyle(@param);
	
	if ($tag eq "Op")
	{
		$rest =~ s/ //;	# remove first space
		return " \\fP[$rest]$term";
	}
	elsif ($tag eq "Xr")	# cross reference
	{
		my $p = shift @param;
		my $url = $p;
		if (@param==1)
		{
			$url .= ".".$param[0];
			$rest = "(".$param[0].")";
		}
		else
		{
			$rest = &mdocStyle(@param);
		}
		if ($cmdLineMode)
		{
			return " <B>".$p."</B>".$rest.$term;
		}
		else
		{
			return " <A HREF=\"".$root."/".$url."\">".$p."</A>".$rest.$term;
		}
	}
	elsif ($tag eq "Fl")
	{
		my ($sofar);
		while (@param)
		{
			$f = shift @param;
			if ($f eq "Ns") # no space
			{
				chop $sofar;
			}
			elsif (&mdocCallable($f))
			{
				unshift @param, $f;
				return $sofar.&mdocStyle(@param).$term;
			}
			else
			{
				$sofar .= "-<B>$f</B> "
			}
		}
		return $sofar.$term;
	}
	elsif ($tag eq "Pa" || $tag eq "Er" || $tag eq "Fn" || $tag eq "Dv")
	{
		return "\\fC$rest\\fP$term";
	}
	elsif ($tag eq "Ad" || $tag eq "Ar" || $tag eq "Em" || $tag eq "Fa" || $tag eq "St" ||
		$tag eq "Ft" || $tag eq "Va" || $tag eq "Ev" || $tag eq "Tn" || $tag eq "%T")
	{
		return "\\fI$rest\\fP$term";
	}
	elsif ($tag eq "Nm")
	{
		$defaultNm = $param[0] unless ($defaultNm);
		$rest = $defaultNm unless ($param[0]);
		return "\\fB$rest\\fP$term";
	}
	elsif ($tag eq "Ic" || $tag eq "Cm" || $tag eq "Sy")
	{
		return "\\fB$rest\\fP$term";
	}
	elsif ($tag eq "Ta")		# Tab
	{
		# Tabs are used inconsistently so this is the best we can do. Columns won't line up. Tough.
		return "&nbsp; &nbsp; &nbsp; $rest$term";
	}
	elsif ($tag eq "Ql")
	{
		$rest =~ s/ //;
		return "`<TT>$rest</TT>'$term";
	}
	elsif ($tag eq "Dl")
	{
		return "<P>&nbsp; &nbsp; <TT>$rest</TT>$term<P>\n";
	}
	elsif ($tag =~ m/^[ABDEOPQS][qoc]$/)
	{
		$lq = "";
		$rq = "";
		if ($tag =~ m/^A/)
			{ $lq = "&lt;"; $rq = "&gt;"; }
		elsif ($tag =~ m/^B/)
			{ $lq = "["; $rq = "]"; }
		elsif ($tag =~ m/^D/)
			{ $lq = "\""; $rq = "\""; }
		elsif ($tag =~ m/^P/)
			{ $lq = "("; $rq = ")"; }
		elsif ($tag =~ m/^Q/)
			{ $lq = "\""; $rq = "\""; }
		elsif ($tag =~ m/^S/)
			{ $lq = "\\'"; $rq = "\\'"; }
		elsif ($tag =~ m/^O/)
			{ $lq = "["; $rq = "]"; }
		if ($tag =~ m/^.o/)
			{ $rq = ""; }
		if ($tag =~ m/^.c/)
			{ $lq = ""; }
		$rest =~ s/ //;
		return $lq.$rest.$rq.$term ;
	}
	elsif (&mdocCallable($tag))	# but not in list above...
	{
		return $rest.$term;
	}
	elsif ($tag =~ m/^[.,;:()\[\]]$/)	# punctuation
	{
		return $tag.$rest.$term;
	}
	elsif ($tag eq "Ns")
	{
		return $rest.$term;
	}
	else
	{
		return " ".$tag.$rest.$term;
	}
}

# Determine if a macro is mdoc parseable/callable
sub mdocCallable
{
	return ($_[0] =~ m/^(Op|Fl|Pa|Er|Fn|Ns|No|Ad|Ar|Xr|Em|Fa|Ft|St|Ic|Cm|Va|Sy|Nm|Li|Dv|Ev|Tn|Pf|Dl|%T|Ta|Ql|[ABDEOPQS][qoc])$/);
}


# Estimate the output width of a string
sub width
{
	local($word) = $_[0];
	$word =~ s,<[/A-Z][^>]*>,,g;		# remove any html tags
	$word =~ s/^\.\S+\s//;
	$word =~ s/\\..//g;
	$x = length($word);
	$word =~ s/[ ()|.,!;:"']//g;	# width of punctuation is about half a character
	return ($x + length($word)) / 2;
}

# Process a tbl table (between TS/TE tags)
sub processTable
{
	if ($troffTable == "1")
	{
		@troffRowDefs = ();
		@tableRows = ();
		$hadUnderscore = 0;
		while(1)
		{
			outputOrigLine();
			if (m/;\s*$/)
			{
				$troffSeparator = quotemeta($1) if (m/tab\s*\((.)\)/);
			}
			else
			{
				s/\.\s*$//;
				s/\t/ /g;
				s/^[^lrcan^t]*//;	# remove any 'modifiers' coming before tag
				# delimit on tags excluding s (viewed as modifier of previous column)
				s/([lrcan^t])/\t$1/g;
				s/^\t//;
				push @troffRowDefs, $_;
				last if ($origLine =~ m/\.\s*$/);
			}
			$_ = getLine();
			preProcessLine();
		}
		$troffTable = 2;
		return;
	}

	s/$troffSeparator/\t/g;
	if ($_ eq ".TE")
	{
		endTblRow();
		flushTable();
		$troffTable = 0;
		plainOutput("</TABLE></BLOCKQUOTE>\n");
	}
	elsif ($_ eq ".T&")
	{
		endTblRow();
		flushTable();
		$troffTable = 1;
	}
	elsif (m/[_=]/ && m/^[_=\t]*$/ && $troffCol==0)
	{
		if (m/^[_=]$/)
		{
			flushTable();
			plainOutput("<TR></TR><TR></TR>\n");
			$hadUnderscore = 1;
		}
		elsif ($troffCol==0 && @troffRowDefs)
		{
			# Don't output a row, but this counts as a row as far as row defs go
			$rowDef = shift @troffRowDefs;
			@troffColDefs = split(/\t/, $rowDef);
		}
	}
	elsif (m/^\.sp/ && $troffCol==0 && !$hadUnderscore)
	{
		flushTable();
		plainOutput("<TR></TR><TR></TR>\n");
	}
	elsif ($_ eq ".br" && $troffMultiline)
	{
		$rowref->[$troffCol] .= "<BR>\n";
	}
	elsif ($_ !~ m/^\./)
	{
		$rowref = $tableRows[$#tableRows];	# reference to current row (last row in array)
		if ($troffCol==0 && @troffRowDefs)
		{
			$rowDef = shift @troffRowDefs;
			if ($rowDef =~ m/^[_=]/)
			{
				$xxx = $_;
				flushTable();
				plainOutput("<TR></TR><TR></TR>\n");
				$hadUnderscore = 1;
				$_ = $xxx;
				$rowDef = shift @troffRowDefs;
			}
			@troffColDefs = split(/\t/, $rowDef);
		}

		if ($troffCol == 0 && !$troffMultiline)
		{
			$rowref = [];
			push(@tableRows, $rowref);
			#plainOutput("<TR valign=top>");
		}

		#{
		if (m/T}/)
		{
			$troffMultiline = 0;
		}
		if ($troffMultiline)
		{
			$rowref->[$troffCol] .= "$_\n";
			return;
		}

		@columns = split(/\t/, $_);
		plainOutput("<!-- Adding (".join(",", @columns)."), type (".join(",", @troffColDefs).") -->\n") if ($debug);
		while ($troffCol <= $#troffColDefs && @columns > 0)
		{
			$def = $troffColDefs[$troffCol];
			$col = shift @columns;
			$col =~ s/\s*$//;
			$align = "";
			$col = "\\^" if ($col eq "" && $def =~ m/\^/);
			$col = "&nbsp;" if ($col eq "");
			$style1 = "";
			$style2 = "";
			if ($col ne "\\^")
			{
				if ($def =~ m/[bB]/ || $def =~ m/f3/)
					{ $style1 = "\\fB"; $style2 = "\\fP"; }
				if ($def =~ m/I/ || $def =~ m/f2/)
					{ $style1 = "\\fI"; $style2 = "\\fP"; }
			}
			if ($def =~ m/c/) { $align = " align=center"; }
			if ($def =~ m/[rn]/) { $align = " align=right"; }
			$span = $def;
			$span =~ s/[^s]//g;
			if ($span) { $align.= " colspan=".(length($span)+1); }

			#{
			if ($col =~ m/T}/)
			{
				$rowref->[$troffCol] .= "$style2</TD>";
				++$troffCol;
			}
			elsif ($col =~ m/T{/) #}
			{
				$col =~ s/T{//; #}
				$rowref->[$troffCol] = "<TD$align>$style1$col";
				$troffMultiline = 1;
			}
			else
			{
				$rowref->[$troffCol] = "<TD$align>$style1$col$style2</TD>";
				++$troffCol;
			}
		}

		endTblRow() unless ($troffMultiline);
	}
}

sub endTblRow
{
	return if ($troffCol == 0);
	while ($troffCol <= $#troffColDefs)
	{
		$rowref->[$troffCol] = "<TD>&nbsp;</TD>";
		#print OUT "<TD>&nbsp;</TD>";
		++$troffCol;
	}
	$troffCol = 0;
	#print OUT "</TR>\n"
}

sub flushTable
{
	plainOutput("<!-- flushTable $#tableRows rows -->\n") if ($debug);

	# Treat rows with first cell blank or with more than one vertically
	# spanned row as a continuation of the previous line.
	# Note this is frequently a useful heuristic but isn't foolproof.
	for($r=0; $r<$#tableRows; ++$r)
	{
		$vspans = 0;
		for ($c=0; $c<=$#{$tableRows[$r+1]}; ++$c)
			{++$vspans if ($tableRows[$r+1][$c] =~ m,<TD.*?>\\\^</TD>,);}
		if ((($vspans>1) || ($tableRows[$r+1][0] =~ m,<TD.*?>&nbsp;</TD>,)) &&
			$#{$tableRows[$r]} == $#{$tableRows[$r+1]}  && 0)
		{
			if ($debug)
			{
				plainOutput("<!-- merging row $r+1 into previous -->\n");
				plainOutput("<!-- row $r: (".join(",", @{$tableRows[$r]}).") -->\n");
				plainOutput("<!-- row $r+1: (".join(",", @{$tableRows[$r+1]}).") -->\n");
			}
			for ($c=0; $c<=$#{$tableRows[$r]}; ++$c)
			{
				$tableRows[$r][$c] .= $tableRows[$r+1][$c];
				$tableRows[$r][$c] =~ s,\\\^,,g;	# merging is stronger than spanning!
				$tableRows[$r][$c] =~ s,</TD><TD.*?>,<BR>,;
			}
			@tableRows = (@tableRows[0..$r], @tableRows[$r+2 .. $#tableRows]);
			--$r;	# process again
		}
	}

	# Turn \^ vertical span requests into rowspan tags
	for($r=0; $r<$#tableRows; ++$r)
	{
		for ($c=0; $c<=$#{$tableRows[$r]}; ++$c)
		{
			$r2 = $r+1;
			while ( $r2<=$#tableRows && ($tableRows[$r2][$c] =~ m,<TD.*?>\\\^</TD>,) )
			{
				++$r2;
			}
			$rs = $r2-$r;
			if ($rs > 1)
			{
				plainOutput("<!-- spanning from $r,$c -->\n") if ($debug);
				$tableRows[$r][$c] =~ s/<TD/<TD rowspan=$rs/;
			}
		}
	}

	# As tbl and html differ in whether they expect spanned cells to be
	# supplied, remove any cells that are 'rowspanned into'.
	for($r=0; $r<=$#tableRows; ++$r)
	{
		for ($c=$#{$tableRows[$r]}; $c>=0; --$c)
		{
			if ($tableRows[$r][$c] =~ m/<TD rowspan=(\d+)/)
			{
				for ($r2=$r+1; $r2<$r+$1; ++$r2)
				{
					$rowref = $tableRows[$r2];
					plainOutput("<!-- removing $r2,$c: ".$rowref->[$c]." -->\n") if ($debug);
					@$rowref = (@{$rowref}[0..$c-1], @{$rowref}[$c+1..$#$rowref]);
				}
			}
		}
	}

	# Finally, output the cells that are left
	for($r=0; $r<=$#tableRows; ++$r)
	{
		plainOutput("<TR valign=top>\n");
		for ($c=0; $c <= $#{$tableRows[$r]}; ++$c)
		{
			outputLine($tableRows[$r][$c]);
		}
		plainOutput("</TR>\n");
	}
	@tableRows = ();
	$troffCol = 0;
	plainOutput("<!-- flushTable done -->\n") if ($debug);
}


# Use these for all font changes, including .ft, .ps, .B, .BI, .SM etc.
# Need to add a mechanism to stack up these changes so tags match: <X> <Y> ... </Y> </X> etc.

sub pushStyle
{
	$result = "";
	$type = $_[0];
	$tag = $_[1];
	print OUT "<!-- pushStyle $type($tag) [".join(",", @styleStack)."] " if ($debug>1);
	@oldItems = ();
	if (grep(m/^$type/, @styleStack))
	{
		print OUT "undoing up to old $type " if ($debug>1);
		while (@styleStack)
		{
			# search back, undoing intervening tags in reverse order
			$oldItem = pop @styleStack;
			($oldTag) = ($oldItem =~ m/^.(\S+)/);
			$result .= "</$oldTag>";
			if (substr($oldItem,0,1) eq $type)
			{
				print OUT "found $oldItem " if ($debug>1);
				while (@oldItems)
				{
					# restore the intermediates again
					$oldItem = shift @oldItems;
					push(@styleStack, $oldItem);
					$result .= "<".substr($oldItem,1).">";
				}
				last;
			}
			else
			{
				unshift(@oldItems, $oldItem);
			}
		}
	}
	print OUT "oldItems=(@oldItems) " if ($debug>1);
	push(@styleStack, @oldItems);	# if we didn't find anything of type
	if ($tag)
	{
		$result .= "<$tag>";
		push(@styleStack, $type.$tag);
	}
	print OUT "-> '$result' -->\n" if ($debug>1);
	return $result;
}

sub resetStyles
{
	if (@styleStack)
	{
		print OUT "<!-- resetStyles [".join(",", @styleStack)."] -->\n";
		print OUT "<HR> resetStyles [".join(",", @styleStack)."] <HR>\n" if ($debug);
	}
	while (@styleStack)
	{
		$oldItem = pop @styleStack;
		($oldTag) = ($oldItem =~ m/^.(\S+)/);
		print OUT "</$oldTag>";
	}
	$currentSize = 0;
	$currentShift = 0;
}

sub blockquote
{
	print OUT "<BLOCKQUOTE>\n";
	++$blockquote;
}

sub endBlockquote
{
	resetStyles();
	while ($blockquote > 0)
	{
		print OUT "</BLOCKQUOTE>\n";
		--$blockquote;
	}
}

sub indent
{
	plainOutput(pushStyle("I", "TABLE"));
	$width = $_[0];
	$width = " width=$width%" if ($width);
	plainOutput("<TR><TD$width>&nbsp;</TD><TD>\n");
}

sub outdent
{
	plainOutput("</TD></TR>\n");
	plainOutput(pushStyle("I"));
}

sub inlineStyle
{
	$_[0] =~ m/^(.)(.*)$/;
	if ($1 eq "f")
		{ fontChange($2); }
	elsif ($1 eq "s" && ! $noFill)
		{ sizeChange($2); }
	else
		{ superSub($1); }
}

sub fontChange
{
	$fnt = $_[0];
	$fnt =~ s/^\(//;

	if ($fnt eq "P" || $fnt eq "R" || $fnt eq "1" || $fnt eq "")
		{ $font = ""; }
	elsif ($fnt eq "B" || $fnt eq "3")
		{ $font = "B"; }
	elsif ($fnt eq "I" || $fnt eq "2")
		{ $font = "I"; }
	else
		{ $font = "TT"; }
	return pushStyle("F", $font);
}

sub sizeChange
{
	$size= $_[0];
	if ($size =~ m/^[+-]/)
		{ $currentSize += $size; }
	else
		{ $currentSize = $size-10; }
	$currentSize = 0 if (! $size);

	$sz = $currentSize;
	$sz = -2 if ($sz < -2);
	$sz = 2 if ($sz > 2);

	if ($currentSize eq "0")
		{ $size = ""; }
	else
		{ $size = "FONT size=$sz"; }
	return pushStyle("S", $size);
}

sub superSub
{
	$sub = $_[0];
	++$currentShift if ($sub eq "u");
	--$currentShift if ($sub eq "d");
	$tag = "";
	$tag = "SUP" if ($currentShift > 0);
	$tag = "SUB" if ($currentShift < 0);
	return pushStyle("D", $tag);
}

sub startNoFill
{
	print OUT "<PRE>\n" unless($noFill);
	$noFill = 1;
}

sub endNoFill
{
	print OUT "</PRE>\n" if ($noFill);
	$noFill = 0;
}


sub processEqns
{
	if ($eqnMode==2 && $_[0] =~ m/^\.EN/)
	{
		$eqnMode = 0;
		outputLine(flushEqn());
		plainOutput("\n");
		return;
	}
	$eqnBuffer .= $_[0]." ";
}

sub processEqnd
{
	processEqns(@_);
	return flushEqn();
}

sub flushEqn
{
	@p = grep($_ !~ m/^ *$/, split(/("[^"]*"|\s+|[{}~^])/, $eqnBuffer) );
	$eqnBuffer = "";
	#return "[".join(',', @p)." -> ".&doEqn(@p)."]\n";
	$res = &doEqn(@p);
	#$res =~ s,\\\((..),$special{$1}||"\\($1",ge;
	#$res =~ s,<,&lt;,g;
	#$res =~ s,>,&gt;,g;
	return $res;
}

sub doEqn
{
	my @p = @_;
	my $result = "";
	my $res;
	my $c;
	while (@p)
	{
		($res, @p) = doEqn1(@p);
		$result .= $res;
	}
	return $result;
}

sub doEqn1
{
	my @p = @_;
	my $res = "";
	my $c;

	$c = shift @p;
	if ($eqndefs{$c})
	{
		@x = split(/\0/, $eqndefs{$c});
		unshift @p, @x;
		$c = shift @p;
	}
	if ($c =~ m/^"(.*)"$/)
	{
		$res = $1;
	}
	elsif ($c eq "delim")
	{
		$c = shift @p;
		if ($c eq "off")
		{
			$eqnStart = "";
			$eqnEnd = "";
		}
		else
		{
			$c =~ m/^(.)(.)/;
			$eqnStart = quotemeta($1);
			$eqnEnd = quotemeta($2);
		}
	}
	elsif ($c eq "define" || $c eq "tdefine" || $c eq "ndefine")
	{
		$t = shift @p;
		$d = shift @p;
		$def = "";
		if (length($d) != 1)
		{
			$def = $d;
			$def =~ s/^.(.*)./\1/;
		}
		else
		{
			while (@p && $p[0] ne $d)
			{
				$def .= shift @p;
				$def .= "\0";
			}
			chop $def;
			shift @p;
		}
		$eqndefs{$t} = $def unless ($c eq "ndefine");
	}
	elsif ($c eq "{")
	{
		my $level = 1;
		my $i;
		for ($i=0; $i<=$#p; ++$i)
		{
			++$level if ($p[$i] eq "{");
			--$level if ($p[$i] eq "}");
			last if ($level==0);
		}
		$res = doEqn(@p[0..$i-1]);
		@p = @p[$i+1..$#p];
	}
	elsif ($c eq "sup")
	{
		($c,@p) = &doEqn1(@p);
		$res = "\\u$c\\d";
	}
	elsif ($c eq "to")
	{
		($c,@p) = &doEqn1(@p);
		$res = "\\u$c\\d ";
	}
	elsif ($c eq "sub" || $c eq "from")
	{
		($c,@p) = &doEqn1(@p);
		$res = "\\d$c\\u";
	}
	elsif ($c eq "matrix")
	{
		($c,@p) = &doEqn1(@p);
		$res = "matrix ( $c )";
	}
	elsif ($c eq "bold")
	{
		($c,@p) = &doEqn1(@p);
		$res = "\\fB$c\\fP";
	}
	elsif ($c eq "italic")
	{
		($c,@p) = &doEqn1(@p);
		$res = "\\fI$c\\fP";
	}
	elsif ($c eq "roman")
	{
	}
	elsif ($c eq "font" || $c eq "gfont" || $c eq "size" || $c eq "gsize")
	{
		shift @p;
	}
	elsif ($c eq "mark" || $c eq "lineup")
	{
	}
	elsif ($c eq "~" || $c eq "^")
	{
		$res = " ";
	}
	elsif ($c eq "over")
	{
		$res = " / ";
	}
	elsif ($c eq "half")
	{
		$res = "\\(12";
	}
	elsif ($c eq "prime")
	{
		$res = "\\' ";
	}
	elsif ($c eq "dot")
	{
		$res = "\\u.\\d ";
	}
	elsif ($c eq "dotdot")
	{
		$res = "\\u..\\d ";
	}
	elsif ($c eq "tilde")
	{
		$res = "\\u~\\d ";
	}
	elsif ($c eq "hat")
	{
		$res = "\\u^\\d ";
	}
	elsif ($c eq "bar" || $c eq "vec")
	{
		$res = "\\(rn ";
	}
	elsif ($c eq "under")
	{
		$res = "_ ";
	}
	elsif ( $c eq "sqrt" || $c eq "lim" || $c eq "sum" || $c eq "pile" || $c eq "lpile" ||
			$c eq "rpile" || $c eq "cpile" || $c eq "int" || $c eq "prod" )
	{
		$res = " $c ";
	}
	elsif ($c eq "cdot")
	{
		$res = " . ";
	}
	elsif ($c eq "inf")
	{
		$res = "\\(if";
	}
	elsif ($c eq "above" || $c eq "lcol" || $c eq "ccol")
	{
		$res = " ";
	}
	elsif ($c eq "sin" || $c eq "cos" || $c eq "tan" || $c eq "log" || $c eq "ln" )
	{
		$res = " $c ";
	}
	elsif ($c eq "left" || $c eq "right" || $c eq "nothing")
	{
	}
	elsif ($c =~ m/^[A-Za-z]/)
	{
		$res = "\\fI$c\\fP";
	}
	else
	{
		$res = $c;
	}

	return ($res, @p);
}

##### Search manpath and initialise special char array #####

sub initialise
{
	# Parse the macro definition file for section names
	if (open(MACRO, "/usr/lib/tmac/tmac.an") ||
		open(MACRO, "/usr/lib/tmac/an") ||
		open(MACRO, "/usr/lib/groff/tmac/tmac.an") ||
		open(MACRO, "/usr/share/tmac/tmac.an") ||
		open(MACRO, "/usr/share/groff/tmac/tmac.an") )
	{
		while (<MACRO>)
		{
			chop;
			if (m/\$2'([0-9a-zA-Z]+)' .ds ]D (.*)$/)
			{
				$sn = $2;
				unless ($sn =~ m/[a-z]/)
				{
					$sn = "\u\L$sn";
					$sn =~ s/ (.)/ \u\1/g;
				}
				$sectionName{"\L$1"} = $sn;
			}
			if (m/\$1'([^']+)' .ds Tx "?(.*)$/)
			{
				$title{"$1"} = $2;
			}
			if (m/^.ds ]W (.*)$/)
			{
				$osver = $1;
			}
		}
	}
	else
	{
		print STDERR "Failed to read tmac.an definitions\n" unless ($cgiMode);
	}
	if (open(MACRO, "/usr/lib/tmac/tz.map"))
	{
		while (<MACRO>)
		{
			chop;
			if (m/\$1'([^']+)' .ds Tz "?(.*)$/)
			{
				$title{"$1"} = $2;
			}
		}
	}

	# Prevent redefinition of macros that have special meaning to us
	$reservedMacros = '^(SH|SS|Sh|Ss)$';

	# Predefine special number registers
	$number{'.l'} = 75;

	# String variables defined by man package
	$vars{'lq'} = '&#147;';
	$vars{'rq'} = '&#148;';
	$vars{'R'} = '\\(rg';
	$vars{'S'} = '\\s0';

	# String variables defined by mdoc package
	$vars{'Le'} = '\\(<=';
	$vars{'<='} = '\\(<=';
	$vars{'Ge'} = '\\(>=';
	$vars{'Lt'} = '<';
	$vars{'Gt'} = '>';
	$vars{'Ne'} = '\\(!=';
	$vars{'>='} = '\\(>=';
	$vars{'q'} = '&#34;';	# see also special case in preProcessLine
	$vars{'Lq'} = '&#147;';
	$vars{'Rq'} = '&#148;';
	$vars{'ua'} = '\\(ua';
	$vars{'ga'} = '\\(ga';
	$vars{'Pi'} = '\\(*p';
	$vars{'Pm'} = '\\(+-';
	$vars{'Na'} = 'NaN';
	$vars{'If'} = '\\(if';
	$vars{'Ba'} = '|';

	# String variables defined by ms package (access to accented characters)
	$vars{'bu'} = '&#187;';
	$vars{'66'} = '&#147;';
	$vars{'99'} = '&#148;';
	$vars{'*!'} = '&#161;';
	$vars{'ct'} = '&#162;';
	$vars{'po'} = '&#163;';
	$vars{'gc'} = '&#164;';
	$vars{'ye'} = '&#165;';
	#$vars{'??'} = '&#166;';
	$vars{'sc'} = '&#167;';
	$vars{'*:'} = '&#168;';
	$vars{'co'} = '&#169;';
	$vars{'_a'} = '&#170;';
	$vars{'<<'} = '&#171;';
	$vars{'no'} = '&#172;';
	$vars{'hy'} = '&#173;';
	$vars{'rg'} = '&#174;';
	$vars{'ba'} = '&#175;';
	$vars{'de'} = '&#176;';
	$vars{'pm'} = '&#177;';
	#$vars{'??'} = '&#178;';
	#$vars{'??'} = '&#179;';
	$vars{'aa'} = '&#180;';
	$vars{'mu'} = '&#181;';
	$vars{'pg'} = '&#182;';
	$vars{'c.'} = '&#183;';
	$vars{'cd'} = '&#184;';
	#$vars{'??'} = '&#185;';
	$vars{'_o'} = '&#186;';
	$vars{'>>'} = '&#187;';
	$vars{'14'} = '&#188;';
	$vars{'12'} = '&#189;';
	#$vars{'??'} = '&#190;';
	$vars{'*?'} = '&#191;';
	$vars{'`A'} = '&#192;';
	$vars{"'A"} = '&#193;';
	$vars{'^A'} = '&#194;';
	$vars{'~A'} = '&#195;';
	$vars{':A'} = '&#196;';
	$vars{'oA'} = '&#197;';
	$vars{'AE'} = '&#198;';
	$vars{',C'} = '&#199;';
	$vars{'`E'} = '&#200;';
	$vars{"'E"} = '&#201;';
	$vars{'^E'} = '&#202;';
	$vars{':E'} = '&#203;';
	$vars{'`I'} = '&#204;';
	$vars{"'I"} = '&#205;';
	$vars{'^I'} = '&#206;';
	$vars{':I'} = '&#207;';
	$vars{'-D'} = '&#208;';
	$vars{'~N'} = '&#209;';
	$vars{'`O'} = '&#210;';
	$vars{"'O"} = '&#211;';
	$vars{'^O'} = '&#212;';
	$vars{'~O'} = '&#213;';
	$vars{':O'} = '&#214;';
	#$vars{'mu'} = '&#215;';
	$vars{'NU'} = '&#216;';
	$vars{'`U'} = '&#217;';
	$vars{"'U"} = '&#218;';
	$vars{'^U'} = '&#219;';
	$vars{':U'} = '&#220;';
	#$vars{'??'} = '&#221;';
	$vars{'Th'} = '&#222;';
	$vars{'*b'} = '&#223;';
	$vars{'`a'} = '&#224;';
	$vars{"'a"} = '&#225;';
	$vars{'^a'} = '&#226;';
	$vars{'~a'} = '&#227;';
	$vars{':a'} = '&#228;';
	$vars{'oa'} = '&#229;';
	$vars{'ae'} = '&#230;';
	$vars{',c'} = '&#231;';
	$vars{'`e'} = '&#232;';
	$vars{"'e"} = '&#233;';
	$vars{'^e'} = '&#234;';
	$vars{':e'} = '&#235;';
	$vars{'`i'} = '&#236;';
	$vars{"'i"} = '&#237;';
	$vars{'^i'} = '&#238;';
	$vars{':i'} = '&#239;';
	#$vars{'??'} = '&#240;';
	$vars{'~n'} = '&#241;';
	$vars{'`o'} = '&#242;';
	$vars{"'o"} = '&#243;';
	$vars{'^o'} = '&#244;';
	$vars{'~o'} = '&#245;';
	$vars{':o'} = '&#246;';
	$vars{'di'} = '&#247;';
	$vars{'nu'} = '&#248;';
	$vars{'`u'} = '&#249;';
	$vars{"'u"} = '&#250;';
	$vars{'^u'} = '&#251;';
	$vars{':u'} = '&#252;';
	#$vars{'??'} = '&#253;';
	$vars{'th'} = '&#254;';
	$vars{':y'} = '&#255;';

	# troff special characters and their closest equivalent

	$special{'em'} = '&#151;';
	$special{'hy'} = '-';
	$special{'\-'} = '&#150;';	# was -
	$special{'bu'} = 'o';
	$special{'sq'} = '[]';
	$special{'ru'} = '_';
	$special{'14'} = '&#188;';
	$special{'12'} = '&#189;';
	$special{'34'} = '&#190;';
	$special{'fi'} = 'fi';
	$special{'fl'} = 'fl';
	$special{'ff'} = 'ff';
	$special{'Fi'} = 'ffi';
	$special{'Fl'} = 'ffl';
	$special{'de'} = '&#176;';
	$special{'dg'} = '&#134;';	# was 182, para symbol
	$special{'fm'} = "\\'";
	$special{'ct'} = '&#162;';
	$special{'rg'} = '&#174;';
	$special{'co'} = '&#169;';
	$special{'pl'} = '+';
	$special{'mi'} = '-';
	$special{'eq'} = '=';
	$special{'**'} = '*';
	$special{'sc'} = '&#167;';
	$special{'aa'} = '&#180;';	# was '
	$special{'ga'} = '&#96;';	# was `
	$special{'ul'} = '_';
	$special{'sl'} = '/';
	$special{'*a'} = 'a';
	$special{'*b'} = '&#223;';
	$special{'*g'} = 'y';
	$special{'*d'} = 'd';
	$special{'*e'} = 'e';
	$special{'*z'} = 'z';
	$special{'*y'} = 'n';
	$special{'*h'} = 'th';
	$special{'*i'} = 'i';
	$special{'*k'} = 'k';
	$special{'*l'} = 'l';
	$special{'*m'} = '&#181;';
	$special{'*n'} = 'v';
	$special{'*c'} = '3';
	$special{'*o'} = 'o';
	$special{'*p'} = 'pi';
	$special{'*r'} = 'p';
	$special{'*s'} = 's';
	$special{'*t'} = 't';
	$special{'*u'} = 'u';
	$special{'*f'} = 'ph';
	$special{'*x'} = 'x';
	$special{'*q'} = 'ps';
	$special{'*w'} = 'o';
	$special{'*A'} = 'A';
	$special{'*B'} = 'B';
	$special{'*G'} = '|\\u_\\d';
	$special{'*D'} = '/&#92;';
	$special{'*E'} = 'E';
	$special{'*Z'} = 'Z';
	$special{'*Y'} = 'H';
	$special{'*H'} = 'TH';
	$special{'*I'} = 'I';
	$special{'*K'} = 'K';
	$special{'*L'} = 'L';
	$special{'*M'} = 'M';
	$special{'*N'} = 'N';
	$special{'*C'} = 'Z';
	$special{'*O'} = 'O';
	$special{'*P'} = '||';
	$special{'*R'} = 'P';
	$special{'*S'} = 'S';
	$special{'*T'} = 'T';
	$special{'*U'} = 'Y';
	$special{'*F'} = 'PH';
	$special{'*X'} = 'X';
	$special{'*Q'} = 'PS';
	$special{'*W'} = 'O';
	$special{'ts'} = 's';
	$special{'sr'} = 'v/';
	$special{'rn'} = '\\u&#150;\\d';	# was 175
	$special{'>='} = '&gt;=';
	$special{'<='} = '&lt;=';
	$special{'=='} = '==';
	$special{'~='} = '~=';
	$special{'ap'} = '&#126;';	# was ~
	$special{'!='} = '!=';
	$special{'->'} = '-&gt;';
	$special{'<-'} = '&lt;-';
	$special{'ua'} = '^';
	$special{'da'} = 'v';
	$special{'mu'} = '&#215;';
	$special{'di'} = '&#247;';
	$special{'+-'} = '&#177;';
	$special{'cu'} = 'U';
	$special{'ca'} = '^';
	$special{'sb'} = '(';
	$special{'sp'} = ')';
	$special{'ib'} = '(=';
	$special{'ip'} = '=)';
	$special{'if'} = 'oo';
	$special{'pd'} = '6';
	$special{'gr'} = 'V';
	$special{'no'} = '&#172;';
	$special{'is'} = 'I';
	$special{'pt'} = '~';
	$special{'es'} = '&#216;';
	$special{'mo'} = 'e';
	$special{'br'} = '|';
	$special{'dd'} = '&#135;';	# was 165, yen
	$special{'rh'} = '=&gt;';
	$special{'lh'} = '&lt;=';
	$special{'or'} = '|';
	$special{'ci'} = 'O';
	$special{'lt'} = '(';
	$special{'lb'} = '(';
	$special{'rt'} = ')';
	$special{'rb'} = ')';
	$special{'lk'} = '|';
	$special{'rk'} = '|';
	$special{'bv'} = '|';
	$special{'lf'} = '|';
	$special{'rf'} = '|';
	$special{'lc'} = '|';
	$special{'rc'} = '|';

	# Not true troff characters but very common typos
	$special{'cp'} = '&#169;';
	$special{'tm'} = '&#174;';
	$special{'en'} = '-';

	# Build a list of directories containing man pages
	@manpath = ();
	if (open(MPC, "/etc/manpath.config") || open(MPC, "/etc/man.config"))
	{
		while (<MPC>)
		{
			if (m/^(MANDB_MAP|MANPATH)\s+(\S+)/)
			{
				push(@manpath, $2);
			}
		}
	}
	@manpath = split(/:/, $ENV{'MANPATH'}) unless (@manpath);
	@manpath = ("/usr/man") unless (@manpath);
}

# Search through @manpath and construct @mandirs (non-empty subsections)
sub loadManDirs
{
	return if (@mandirs);
	print STDERR "Searching ",join(":", @manpath)," for mandirs\n" unless($cgiMode);
	foreach $tld (@manpath)
	{
		$tld =~ m/^(.*)$/;
		$tld = $1;	# untaint manpath
		if (opendir(DIR, $tld))
		{
			# foreach $d (<$tld/man[0-9a-z]*>)
			foreach $d (sort readdir(DIR))
			{
				if ($d =~ m/^man\w/ && -d "$tld/$d")
				{
					push (@mandirs, "$tld/$d");
				}
			}
			closedir DIR;
		}
	}
}

##### Utility to search manpath for a given command #####

sub findPage
{
	$request = $_[0];
	$request =~ s,^/,,;
	@multipleMatches = ();

	$file = $_[0];
	return $file if (-f $file || -f "$file.gz" || -f "$file.bz2");

	# Search the path for the requested man page, which may be of the form:
	# "/usr/man/man1/ls.1", "ls.1" or "ls".
	($page,$sect) = ($request =~ m/^(.+)\.([^.]+)$/);
	$sect = "\L$sect";

	# Search the specified section first (if specified)
	if ($sect)
	{
		foreach $md (@manpath)
		{
			$dir = $md;
			$file = "$dir/man$sect/$page.$sect";
			push(@multipleMatches, $file) if (-f $file || -f "$file.gz" || -f "$file.bz2");
		}
	}
	else
	{
		$page = $request;
	}
	if (@multipleMatches == 1)
	{
		return pop @multipleMatches;
	}

	# If not found need to search through each directory
	loadManDirs();
	foreach $dir (@mandirs)
	{
		($s) = ($dir =~ m/man([0-9A-Za-z]+)$/);
		$file = "$dir/$page.$s";
		push(@multipleMatches, $file) if (-f $file || -f "$file.gz" || -f "$file.bz2");
		$file = "$dir/$request";
		push(@multipleMatches, $file) if (-f $file || -f "$file.gz" || -f "$file.bz2");
		if ($sect && "$page.$sect" ne $request)
		{
			$file = "$dir/$page.$sect";
			push(@multipleMatches, $file) if (-f $file || -f "$file.gz" || -f "$file.bz2");
		}
	}
	if (@multipleMatches == 1)
	{
		return pop @multipleMatches;
	}
	if (@multipleMatches > 1)
	{
		return "";
	}
	# Ok, didn't find it using section numbers. Perhaps there's a page with the
	# right name but wrong section number lurking there somewhere. (This search is slow)
	# eg. page.1x in man1 (not man1x) directory
	foreach $dir (@mandirs)
	{
		opendir(DIR, $dir);
		foreach $f (readdir DIR)
		{
			if ($f =~ m/^$page\./)
			{
				$f =~ s/\.(gz|bz2)$//;
				push(@multipleMatches, "$dir/$f");
			}
		}
	}
	if (@multipleMatches == 1)
	{
		return pop @multipleMatches;
	}
	return "";
}

sub loadPerlPages
{
	my ($dir,$f,$name,@files);
	loadManDirs();
	return if (%perlPages);
	foreach $dir (@mandirs)
	{
		if (opendir(DIR, $dir))
		{
			@files = sort readdir DIR;
			foreach $f (@files)
			{
				next if ($f eq "." || $f eq ".." || $f !~ m/\./);
				next unless ("$dir/$f" =~ m/perl/);
				$f =~ s/\.(gz|bz2)$//;
				($name) = ($f =~ m,(.+)\.[^.]*$,);
				$perlPages{$name} = "$dir/$f";
			}
			closedir DIR;
		}
	}
	delete $perlPages{'perl'}; 	# too ubiquitous to be useful
}

sub fmtTime
{
    my $time = $_[0];
    my @days = qw (Sun Mon Tue Wed Thu Fri Sat);
    my @months = qw (Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec);
    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$istdst) = localtime($time);
    return sprintf ("%s, %02d %s %4d %02d:%02d:%02d GMT",
         $days[$wday],$mday,$months[$mon],1900+$year,$hour,$min,$sec);
}

