#!/usr/bin/perl -w
#
# insert html into a cpp file containing begin and end marker comments.
#   $ARGV[0];			# insert this html file ...
#   $ARGV[1];			# ... between here ...
#   $ARGV[2];			# ... and here ...
#   $ARGV[3];			# ... within this file
# also convert VVVV in this file to a version number based on the current time.
#
# N.B. All literal strings must use apostrophe, double quotes are retained by preceding with \
# N.B. max size of an F() string seems to be about 2KB

use strict;
use warnings;

# insure three args
if (@ARGV != 4) {
    printf "Usage: x.html marker1 marker2 x.cpp\n";
    exit 1;
}

# handy args
my $htmlfn = $ARGV[0];			# insert this html file ...
my $marker1 = $ARGV[1];			# ... between here ...
my $marker2 = $ARGV[2];			# ... and here ...
my $editfn = $ARGV[3];			# ... within this file

# tmp file
my $tmpfn = ".Webpage.cpp";

# we insist editfn is writable
my ($dev,$ino,$mode,$xxx) = stat ($editfn);
defined($mode) or die "Can not stat $editfn: $!\n";
($mode & 200) or die "$editfn must be writable\n";

# open edit file
open EF, "$editfn" or die "Can not open $editfn: $!\n";

# open html file
open HF, "$htmlfn" or die "Can not open $htmlfn: $!\n";

# create temp file
open TF, ">$tmpfn" or die "Can not create $tmpfn: $!\n";

# copy edit to tmp up through the first magic line, inclusive
my $foundm1 = 0;
while (<EF>) {
    print TF;
    if (/$marker1/) {
	$foundm1 = 1;
	last;
    }
}
if (!$foundm1) {
    printf ("Can not find $marker1 in $editfn\n");
    exit 1;
}

# now copy the modifed html into tmp
print TF "        client.print (F(\n";

my $nchars = 0;
while(<HF>) {
    chomp();
    $_ =~ s/\\/\\\\/g;		# retain all \ by turning into \\
    $_ =~ s/"/\\"/g;		# retain all " by turning into \"
    $_ =~ s/\t/        /g;	# expand tabs
    if (/VVVV/) {
	my $version = `date -u +'%Y%m%d%H'`;
	chomp($version);
	$_ =~ s/VVVV/$version/;
    }
    print TF "            \"$_ \\r\\n\"\n";
    $nchars += length();
    if ($nchars > 2000) {
	print TF "        ));\n";
	print TF "        client.print (F(\n";
	$nchars = 0;
    }
}

print TF "        ));\n";

# discard editfn down to second magic line, again inclusive
my $foundm2 = 0;
while (<EF>) {
    if (/$marker2/) {
	print TF;
	$foundm2 = 1;
	last;
    }
}
if (!$foundm2) {
    printf ("Can not find $marker2 in $editfn\n");
    exit 1;
}

# copy remainder of editfn to tmp
while (<EF>) {
    print TF;
}

# close all files and replace edit with tmp
close HF;
close EF;
close TF;
unlink ($editfn);
rename ($tmpfn, $editfn);
