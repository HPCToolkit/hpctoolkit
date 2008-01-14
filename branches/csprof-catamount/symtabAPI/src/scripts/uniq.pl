#!/usr/bin/perl

$i = 1;
$prev = <>;
while (<>) {
	if ($prev eq $_) {
		# Advance counter if duplicate found.
		$i++;

	} elsif ($i > 1) {
		# Print consolidated line.
		print "($i) $prev";
		$prev = $_;
		$i = 1;

	} else {
		# Print line and reset counter.
		print $prev;
		$prev = $_;
		$i = 1;
	}
}
if ($prev eq $_) {
	$i++;
	print "($i) $prev";

} else {
	print $prev;
}
