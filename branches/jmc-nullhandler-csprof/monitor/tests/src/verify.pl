#!/usr/bin/perl -w -s

my $test_cnt = 0;
my $err = 0;
my $current = "";

$help ||= 0; ${-help} ||=0; $h ||=0; ${-h} ||=0;

if ($help || ${-help} || $h || ${-h} || !@ARGV) {
  print "Usage: $0 [-hv] test ...\n";
  print " -h Print this message.\n";
  print " -v Verbose output.\n";
  exit(1);
}

TEST: for $base (@ARGV) {
  $current = $base;
  $run_out = "$base.out";
  print "Processing $base\n" if ($v);
  $output = `cat $run_out` || die "Could not open $run_out for reading: $!\n" ;
  @regex  = split /\n/, `cat $base.regex` || die "Could not open $base.regex for reading: $!\n" ;

  $line = 0;
REGEX:  for $exp (@regex) {
    chomp($exp);
    next REGEX if ($exp =~ m/^\s+$/); # skip blank lines
    next REGEX if ($exp =~ m/^#.*$/); # skip commented lines
    print "\tSearching for $exp .. " if ($v);
    $test_cnt++;
    $line++;
    if ($output =~ /$exp/mg) {
      print "found\n" if ($v);
      next REGEX;
    }
    else {
      print "FAILED\n" if ($v);
      print STDERR "\t$current: ERROR, expression on line $line could not be found!\n";
      print STDERR "\t$current: ERROR, expression is ($exp)\n";
      print STDERR "\t$current: ERROR, please examine $run_out and $base.regex\n";
      $err++;
      next REGEX;
    }
  }
  if ($v) {
      my $passed = $test_cnt - $err;
      print STDOUT "$current: $test_cnt tests\n";
      print STDOUT "$current: $passed passed\n";
      print STDOUT "$current: $err failed\n";
  }
    if ($err) {
	print STDOUT "$current: FAILED\n";
    }
    else {
	print STDOUT "$current: PASSED\n"; 
    }
}
exit($err);

