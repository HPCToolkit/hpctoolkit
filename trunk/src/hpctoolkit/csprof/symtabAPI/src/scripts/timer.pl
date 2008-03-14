#!/usr/bin/perl

use strict;
use POSIX;

sub final_reap();
sub reap_children();
sub set_reap($);
sub catch_child($);

my $timeLimit = 60 * 5; # Default 5 minute wait.
my @regexList;

my $childPid = 0;
my $childStatus = 'UNDEF';
my $retVal = -1;

$SIG{ALRM} = \&set_reap;
$SIG{KILL} = \&set_reap;
$SIG{INT}  = \&set_reap;
$SIG{CHLD} = \&catch_child;

if ($#ARGV < 0) {
    print(STDERR "timer.pl Usage:\n");
    print(STDERR "\t./timer.pl [ -t time ] [ -r regex ] program [ arguments ]\n");
    print(STDERR "\t    -t time\tTime limit in seconds.  Default = $timeLimit.\n");
    print(STDERR "\t    -r regex\tRegex of processes to consider as children.\n");
    print(STDERR "\n");
    print(STDERR "timer.pl Description:\n");
    print(STDERR "\tForks a process which executes the given program for\n");
    print(STDERR "\ta limited amount of time.  If the child process does\n");
    print(STDERR "\tnot terminate within the time limit, timer.pl will\n");
    print(STDERR "\tsend it a SIGTERM, followed by a SIGKILL.\n");
    print(STDERR "\n");
    exit(-1);
}


while ($ARGV[0] =~ /^-/) {
    if ($ARGV[0] eq '-t') {
	shift;
	$timeLimit = shift;

    } elsif ($ARGV[0] eq '-r') {
	shift;
	push(@regexList, shift);

    } else {
	print(STDERR "Unknown option: $ARGV[0]\n");
	exit(-1);
    }
}

if (@regexList == undef) {
    # Default child regex list.
    # ** Should/Can this regex be more specific?
    @regexList = ('^\S*test\d+.mutatee',
		  '^\S*dyncov',
		  '^\S*ijpeg',
		  '^\S*cc1'
		  );
}

$childPid = fork();
die("*** Could not fork: $!") if (!defined $childPid);

if ($childPid == 0) {
    #
    # Child Case
    #
    my $group = setsid(); # Start a new process group.
    exec(@ARGV) or die("*** Could not execute '$ARGV[0]': $!\n");

} else {
    #
    # Parent Case
    #
#   select((select(FILE), $| = 1)[0]);
    alarm($timeLimit);
    pause() if ($childStatus eq 'UNDEF');  # Wait for signal (if needed).

    # At this point, $childStatus should be one of the following:
    #
    # 'NORMAL': Child exited normally.  $retVal holds return value.
    # 'SIGNAL': Child exited via signal.  $retVal holds signal number.
    # 'CORE':   Child dumped core.
    # 'ALRM':   Child exceeded time limit.  Parent should reap children.
    # 'KILL':   Parent received SIGKILL.  Parent should reap children.
    # 'INT':    Parent received SIGINT.  Parent should reap children.

    reap_children() if ($childStatus eq 'ALRM' ||
			$childStatus eq 'KILL' ||
			$childStatus eq 'INT');

    if ($childStatus eq 'SIGNAL') {
	print(STDERR "*** Child terminated abnormally via signal $retVal.\n");
	$retVal = -2;

    } elsif ($childStatus eq 'CORE') {
	print(STDERR "*** Child terminated abnormally and dumped core.\n");
	$retVal = -2;
    }

    final_reap();
    exit($retVal);
}

######################################################
# Called if timer expires, or if interrupted by user.
#
sub set_reap($) {
    return if ($childStatus ne 'UNDEF');
    $childStatus = shift;
    $retVal = -1;
}

######################################################
# Called if child returns normally.
#
sub catch_child($) {
    my $result = -1;
    my $status = -1;
    return if ($childStatus ne 'UNDEF');

    $result = waitpid(-1, &WNOHANG);
    $status = $?;

    if ($result ==  $childPid) {
      if ($? & 127) {
	  $childStatus = 'SIGNAL';
	  $retVal = ($? & 127);
      } elsif ($? & 128) {
	  $childStatus = 'CORE';
      } else {
  	$childStatus = 'NORMAL';
	$retVal = ($? >> 8);
      }
    } 
}

######################################################
# Reap children via process group.
#
sub reap_children() {
    my $children;

    if ($childStatus eq 'ALRM') {
	print(STDERR "*** Process exceeded time limit.  Reaping children.\n");
    } else {
	print(STDERR "*** SIG$childStatus received.  Reaping children.\n");
    }
    $children = kill(SIGTERM, -$childPid);
    return if ($children == 0);
    sleep(5);

    foreach (1 .. 3) {
	$children = kill(SIGKILL, -$childPid);
	last if ($children == 0);
	sleep(3);
    }
}

######################################################
# Reap children via external 'ps'.
#
sub final_reap()
{
    my $ps = "/bin/ps -u $<";	# "$<" == Effective UID

    foreach (1 .. 5) {
	my $done = 1;
	my @input = `$ps`;	# Run "ps".
	chomp(@input);		# Remove trailing '\n'.

	# Find start of command column via 'ps' header
	my $cmdIdx = rindex(shift(@input), 'C');
	die "Cannot parse '$ps' output.  Exiting." if ($cmdIdx < 0);

	foreach (@input) {
	    # Parse 'pid' and 'command' from line.
	    my ($pid, $aixPid) = split(' ', $_, 3);
	    my $cmd = substr($_, $cmdIdx);

	    $pid = $aixPid if ($ENV{"PLATFORM"} =~ /^rs6000-ibm-aix/);
	    foreach my $pattern (@regexList) {
		if ($cmd =~ /$pattern/) {
		    print(STDERR "*** Extra process found;  Reaping $pid ($cmd).\n");
		    kill(SIGKILL, $pid);
		    kill(SIGCONT, $pid);
		    $done = 0;
		}
	    }
	}

	# Wait 3 seconds if we sent any signals.
	sleep(3) if ($done == 0);

	last if ($done == 1);
    }
}
