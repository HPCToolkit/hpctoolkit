## $Id$
## * BeginRiceCopyright *****************************************************
## 
## Copyright ((c)) 2002, Rice University 
## All rights reserved.
## 
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are
## met:
## 
## * Redistributions of source code must retain the above copyright
##   notice, this list of conditions and the following disclaimer.
## 
## * Redistributions in binary form must reproduce the above copyright
##   notice, this list of conditions and the following disclaimer in the
##   documentation and/or other materials provided with the distribution.
## 
## * Neither the name of Rice University (RICE) nor the names of its
##   contributors may be used to endorse or promote products derived from
##   this software without specific prior written permission.
## 
## This software is provided by RICE and contributors "as is" and any
## express or implied warranties, including, but not limited to, the
## implied warranties of merchantability and fitness for a particular
## purpose are disclaimed. In no event shall RICE or contributors be
## liable for any direct, indirect, incidental, special, exemplary, or
## consequential damages (including, but not limited to, procurement of
## substitute goods or services; loss of use, data, or profits; or
## business interruption) however caused and on any theory of liability,
## whether in contract, strict liability, or tort (including negligence
## or otherwise) arising in any way out of the use of this software, even
## if advised of the possibility of such damage. 
## 
## ******************************************************* EndRiceCopyright *

# ----------------------------------------------------------------------
#
# XML writer
#
# ----------------------------------------------------------------------


# ----------------------------------------------------------------------
# -- writeContent($outFileHandle, $content) --
#   writes some content to $outFileHandle
# ----------------------------------------------------------------------

sub writeContent {
    my ($outFileHandle, $content) = @_;
    print {$outFileHandle} $content;
}

# ----------------------------------------------------------------------
# -- writeNewLine($outFileHandle) --
#   writes a new line to $outFileHandle
# ----------------------------------------------------------------------

sub writeNewLine {
    my $outFileHandle = shift;
    print {$outFileHandle} "\n";
}


# ----------------------------------------------------------------------
#
# Header related XML writers
#
# ----------------------------------------------------------------------

my $headerBeginTag = "<PROFILEHEADER>";
my $headerEndTag   = "</PROFILEHEADER>";

# ----------------------------------------------------------------------
# -- writeHeaderBeginTag($outFileHandle) --
#   writes an XML header begin tag
# ----------------------------------------------------------------------

sub writeHeaderBeginTag {
    my ($outFileHandle) = @_;
    print {$outFileHandle} $headerBeginTag;
}

# ----------------------------------------------------------------------
# -- writeHeaderEndTag($outFileHandle) --
#   writes an XML header end tag
# ----------------------------------------------------------------------

sub writeHeaderEndTag {
    my ($outFileHandle) = @_;
    print {$outFileHandle} $headerEndTag;
}

# ----------------------------------------------------------------------
#
# parameters related XML writers
#
# ----------------------------------------------------------------------

my $paramsStartTag      = "<PROFILEPARAMS>";
my $paramsEndTag        = "</PROFILEPARAMS>";
my $executableFormatTag = "<PROFILEEXECUTABLE name=\"%s\">";
my $experimentFormatTag = "<PROFILEEXPERIMENTNAME native_name=\"%s\" papi_name=\"%s/\" papi_number=\"0x%x\">";
my $marchingFormatTag   = "<PROFILESAMPLINGINFOMARCHINGORDERS sampling_frequency=\"%d\">";
my $scalingFormatTag    = "<PROFILESCALING unit=\"%s\" units_per_sample=\"%d\">";

# ----------------------------------------------------------------------
# -- writeParamsStartTag($outFileHandle) --
#   writes an XML parameters start tag
# ----------------------------------------------------------------------

sub writeParamsStartTag {
    my ($outFileHandle) = @_;
    print {$outFileHandle} $paramsStartTag;
}

# ----------------------------------------------------------------------
# -- writeParamsEndTag($outFileHandle) --
#   writes an XML parameters end tag
# ----------------------------------------------------------------------

sub writeParamsEndTag {
    my ($outFileHandle) = @_;
    print {$outFileHandle} $paramsEndTag;
}

# ----------------------------------------------------------------------
# -- writeExecutable($outFileHandle, $executableName) --
#   writes an XML executable tag
# ----------------------------------------------------------------------

sub writeExecutable {
    my ($outFileHandle, $executableName) = @_;
    printf {$outFileHandle} $executableFormatTag, $executableName;
}

# ----------------------------------------------------------------------
# -- writeExperimentName($outFileHandle, $experimentName) --
#   writes an XML experiment name tag
# ----------------------------------------------------------------------

sub writeExperimentName {
    my ($outFileHandle, $experimentName) = @_;
    my $papiName = $experimentTypes{$experimentName};

    printf {$outFileHandle} ($experimentFormatTag, $experimentName, $papiName,
			     $PapiSpec::symbolValueHashTable{$papiName});
}

# ----------------------------------------------------------------------
# -- writeMarchingOrders($outFileHandle, $marchingOrders) --
#   writes an XML marching orders tag
# ----------------------------------------------------------------------

sub writeMarchingOrders {
    my ($self, $outFileHandle) = @_;
    my $marchingOrders = $self->{marchingOrders};
    
    if ($marchingOrders =~ /$hwcMarchingRE/) {
	print {$outFileHandle} $1, $self->{counterNumber}, $2, $self->{counterOverflowValue}, "\n";
	if ($self->{counterNumber} eq "") {
	    $self->{counterNumber} = $1;
	}
	else {
	    if ($1 != $self->{counterNumber}) {
		die "Inconsistent counter numbers: $1 vs. $self->{counterNumber}\n";
	    }
	}
	
	if ($self->{counterOverflowValue} eq "") {
	    $self->{counterOverflowValue} = $2;
	}
	else {
	    if ($2 != $self->{counterOverflowValue}) {
		die "Inconsistent counter overflow values: $1 vs. $self->{counterOverflowValue}\n";
	    }
	}
    }

    if ($marchingOrders =~ /$hwctMarchingRE/) {
	if ($self->{counterNumber} eq "") {
	    $self->{counterNumber} = $1;
	}
	else {
	    if ($1 ne $self->{counterNumber}) {
		die "Inconsistent counter numbers: $1 vs. $self->{counterNumber}\n";
	    }
	}
	if ($self->{counterOverflowValue} eq "") {
	    $self->{counterOverflowValue} = $2;
	}
	else {
	    if ($2 != $self->{counterOverflowValue}) {
		die "Inconsistent counter overflow values: $1 vs. $self->{counterOverflowValue}\n";
	    }
	}
    }
	
    printf {$outFileHandle} ($marchingFormatTag,
			     $self->{counterOverflowValue});
    writeNewLine($outFileHandle);
    print {$outFileHandle} @{$hardwareCounters[$hwcTypes{$self->{cpuType}}]}, "\n";
    printf {$outFileHandle} ($scalingFormatTag,
			     $hardwareCounters[$hwcTypes{$self->{cpuType}}][$self->{counterNumber}],
			     $self->{counterOverflowValue});
}

1;
