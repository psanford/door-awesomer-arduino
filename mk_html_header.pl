#!/usr/bin/perl

use strict;
use warnings;

use Fcntl qw(SEEK_SET SEEK_END);

my $fh;
open($fh, '<door.html') || die "couldn't open door.html";

my $outbuffer = "";

while (my $line = <$fh>) {
    chomp($line);
    $outbuffer .= $line;
}

$outbuffer =~ s/\s+/ /g;
$outbuffer =~ s/"/'/g;

print 'PROGMEM prog_char body[] = "' . $outbuffer . '";';
print "\n\n";

close($fh);

open($fh, '<title.png') || die "couldn't open title.png";

binmode($fh);

seek($fh, 0, SEEK_END);
my $len = tell($fh);

my $binary = '';

seek($fh, 0, SEEK_SET);

read($fh, $binary, $len);

close($fh);

print "#define TITLE_IMG_LEN $len\n";
print 'PROGMEM prog_char title_image[] = { ';

print '0x' . unpack("H2", substr($binary, 0, 1));

for (my $i = 1; $i < $len; $i++) {
  print ', 0x' . unpack("H2", substr($binary, $i, 1));
}

print " };\n";


