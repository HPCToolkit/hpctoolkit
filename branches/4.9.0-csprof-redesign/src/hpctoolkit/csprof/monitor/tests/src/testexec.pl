#!/usr/bin/perl
$shell = $ENV{"SHELL"};
exec $shell '-sh', @ARGV;      # pretend it's a login shell
die "Could not execute $shell: $!\n";
