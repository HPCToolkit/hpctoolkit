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
# $Id$
# $Author$
# $Date$
# $Revision$
# $Log$
# Revision 1.1  2003/04/21 21:55:04  slindahl
# Initial revision
#
# Revision 1.10  2003/03/03  14:39:36  eraxxon
# New formats for load modules.
#
# Revision 1.9  2003/01/23  14:47:19  eraxxon
# Fix typo in DTD.
#
# Revision 1.8  2003/01/22  21:40:50  eraxxon
# The select attribute of FILE defaults to 0.
#
# Revision 1.7  2003/01/14  15:08:02  eraxxon
# Merge HPCView and HPCTools code; port HPCView to ANSI C++; replace 'Boolean' with C++ builtin 'bool'; move architecture independent types to include/*, etc.
#
# Revision 1.6  2002/07/17  18:08:52  eraxxon
# Update copyright notices and RCS strings.
#
# Revision 1.5  2002/04/12  02:28:50  rjf
# display attribute not written to METRIC any more
#
# Revision 1.4  2002/04/01  15:23:55  eraxxon
# Add version attribute to PROFILE and PGM dtd's.
#
# Revision 1.3  2002/03/26 13:43:01  eraxxon
# Fix error in RE for file names; misc. updates.
#
# Revision 1.3  2000/03/29  20:44:40  dsystem
# change output format to string from integer.
#
# Revision 1.2  2000/02/04 15:50:35  xyuan
# add in dtd info as $PROFILEXMLWriter::PROFILEXmlDtd
#
# Revision 1.1  2000/02/04 15:18:34  xyuan
# Initial revision
#
# ----------------------------------------------------------------------

# ----------------------------------------------------------------------
#
# PROFILEXMLWriter.pm
#   XML style writer module for the profile information of Memory Hierarchy
#
#   -- interface --
#
#    -- constructor --
#
#     new($outFileHandle)
#       create a new PROFILEXMLWriter object
#         $outFileHandle        the output file handle
#
#    -- methods setting values --
#
#     setTargetName($targetName)
#       set the name of the executable
#         $targetName           the executable file name
#
#     setNativeName($nativeName)
#       set the original name of the experiment
#         $nativeName           the native experiment name
#
#     setPeriod($period) 
#       set the sampling period of the metric (experiment)
#         $period               the sampling period
#
#     setDisplayName($displayName)
#       set the display name (for the viewing tool) of the metric
#         $displayName          the display name
#
#     setDisplay($displayName)
#       determine whether this metric should be displayed (on the viewing tool)
#         $display              boolean value
#   
#     -- deprecated -- 
#     setPapiName($papiName)
#       set the PAPI name of the experiment
#         $papiName             the PAPI experiment name
#
#     -- deprecated -- 
#     setPapiNumber($papiNumber)
#       set the PAPI number of the experiment
#         $papiNumber           the PAPI experiment number
#
#    -- methods writing XML document --
#
#     writePROFILEStartTag()
#       write a start tag for PROFILE element
#
#     writePROFILEEndTag()
#       write an end tag for PROFILE element
#
#     writePROFILEHeader($hdrInfoRef)
#       write an PROFILE Header (recording the original header lines of the
#       profile) and Params elements (some extracted experiment parameters)
#         $hdrInfoRef           the original header lines of the profile
#
#     writeScopeTreeStartTag()
#       write a start tag for PROFILESCOPETREE element
#
#     writeScopeTreeEndTag()
#       write an end tag for PROFILESCOPETREE element
#
#     writeSTPgmStartTag($name)
#       write a start tag for PGM element
#         $name                 the program name
#
#     writeSTPgmEndTag()
#       write an end tag for PGM element
#
#     writeSTFileStartTag($name)
#       write a start tag for F (file) element
#         $name                 the file name
#
#     writeSTFileEndTag()
#       write an end tag for F (file) element
#
#     writeSTProcStartTag($name)
#       write a start tag for P (proc) element
#         $name                 the procedure name
#
#     writeSTProcEndTag()
#       write an end tag for P (proc) element
#
#     writeSTStmtStartTag($startLine, $id)
#       write a start tag for S (stmt) element
#         $startLine            starting line number
#         $id                   line id (e.g. loop id)
#
#     writeSTStmtEndTag()
#       write an end tag for S (stmt) element
#
#     writeSTMetricFormatTag()
#       write a tag for M (metric) element
#         $value                metric value (period * count) ???
#         #not needed for one metric: $metric_name
#
# 
# ----------------------------------------------------------------------

package PROFILEXMLWriter;

use FileHandle;


# ----------------------------------------------------------------------
#
# DTD section
#
# ----------------------------------------------------------------------

# from ${HPCTOOLS}/lib/dtd/PROFILE.dtd
# FIXME: it would be much better to include/read the above file

$PROFILEXMLWriter::PROFILEXmlDtd =
"<?xml version=\"1.0\"?>
<!DOCTYPE PROFILE [
<!-- Profile: correlates profiling info with program structure. -->
<!ELEMENT PROFILE (PROFILEHDR, PROFILEPARAMS, PROFILESCOPETREE)>
<!ATTLIST PROFILE
	version CDATA #REQUIRED>
  <!ELEMENT PROFILEHDR (#PCDATA)>
  <!ELEMENT PROFILEPARAMS (TARGET, METRICS)>
    <!ELEMENT TARGET EMPTY>
    <!ATTLIST TARGET
	name CDATA #REQUIRED>
    <!ELEMENT METRICS (METRIC)+>
    <!ELEMENT METRIC EMPTY>
    <!ATTLIST METRIC
	shortName   CDATA #REQUIRED
	nativeName  CDATA #REQUIRED
	period      CDATA #REQUIRED
	units       CDATA #IMPLIED
	displayName CDATA #IMPLIED
	display     (true|false) #IMPLIED>
  <!ELEMENT PROFILESCOPETREE (PGM)*>
    <!-- This is essentially the PGM dtd with M element added. -->
    <!ELEMENT PGM (G|LM|F|M)+>
    <!ATTLIST PGM
	n CDATA #REQUIRED>
    <!-- Groups create arbitrary sets of other elements except PGM. -->
    <!ELEMENT G (G|LM|F|P|L|S|M)*>
    <!ATTLIST G
	n CDATA #IMPLIED>
    <!-- Runtime load modules for PGM (e.g., DSOs, exe) -->
    <!ELEMENT LM (G|F|M)*>
    <!ATTLIST LM
	n CDATA #REQUIRED>
    <!-- Files contain procedures and source line info -->
    <!ELEMENT F (G|P|L|S|M)*>
    <!ATTLIST F
	n CDATA #REQUIRED>
    <!-- Procedures contain source line info 
         n: processed name; ln: link name -->
    <!ELEMENT P (G|L|S|M)*>
    <!ATTLIST P
	n  CDATA #REQUIRED
	ln CDATA #IMPLIED
	b CDATA #IMPLIED
	e CDATA #IMPLIED>
    <!-- Loops -->
    <!ELEMENT L (G|L|S|M)*>
    <!ATTLIST L
	b CDATA #IMPLIED
	e CDATA #IMPLIED>
    <!-- Statement/Statement range -->
    <!ELEMENT S (M)*>
    <!ATTLIST S
	b CDATA #REQUIRED
	e CDATA #IMPLIED
	id CDATA #IMPLIED>
    <!ELEMENT M EMPTY>
    <!ATTLIST M
	n CDATA #REQUIRED
	v CDATA #REQUIRED>
]>
";

# ----------------------------------------------------------------------
#
# the object variables
#
# ----------------------------------------------------------------------

# the fields of the PROFILEXMLWriter object
my %fields = (
	      outFileHandle => undef,
	      profileHeader => undef,
	      );

# a hash table recording some header infos
my %profileHeader = (
		targetName        => undef,
		shortName         => "0", # assume first metric, named 0
		nativeName        => undef,
		period            => undef,
		displayName       => undef,
		display           => undef,
#deprecated	papiName          => undef,
#deprecated	papiNumber        => undef,
		);

# ----------------------------------------------------------------------
# -- new($class, $outFileHandle) --
#   constructor
# ----------------------------------------------------------------------

sub new {
    my ($class, $outFileHandle) = @_;
    my $self = { %fields, };
    $self->{outFileHandle} = $outFileHandle;
    $self->{profileHeader} = { %profileHeader, };
    bless $self, $class;
    return $self;
}

# ----------------------------------------------------------------------
#
# methods setting values
#
# ----------------------------------------------------------------------

# ----------------------------------------------------------------------
# -- setTargetName($targetName) --
#   set the name of the executable
# ----------------------------------------------------------------------

sub setTargetName {
    my ($self, $targetName) = @_;
    # print STDERR $targetName, "\t";
    $self->{profileHeader}->{targetName} = $targetName;
    #print STDERR $self->{profileHeader}->{targetName}, "\n";
}
    
# ----------------------------------------------------------------------
# -- setNativeName($nativeName) --
#   set the native name of the experiment
# ----------------------------------------------------------------------

sub setNativeName {
    my ($self, $nativeName) = @_;
    $self->{profileHeader}->{nativeName} = $nativeName;
}

# ----------------------------------------------------------------------
# -- setPeriod($period) --
#   set the native name of the experiment
# ----------------------------------------------------------------------

sub setPeriod {
    my ($self, $period) = @_;
    $self->{profileHeader}->{period} = $period;
}

# ----------------------------------------------------------------------
# -- setDisplayName($displayName) --
#   set the display name (for the viewing tool) of the metric
# ----------------------------------------------------------------------

sub setDisplayName {
    my ($self, $displayName) = @_;
    $self->{profileHeader}->{displayName} = $displayName;
}

# ----------------------------------------------------------------------
# -- setDisplay(display) --
#   determine whether this metric should be displayed (on the viewing tool)
# ----------------------------------------------------------------------

sub setDisplay {
    my ($self, $display) = @_;
    $self->{profileHeader}->{display} = $display;
}

#     -- deprecated --
# ----------------------------------------------------------------------
# -- setPapiName($papiName) --
#   set the PAPI name of the experiment
# ----------------------------------------------------------------------
#
#sub setPapiName {
#    my ($self, $papiName) = @_;
#    $self->{profileHeader}->{papiName} = $papiName;
#}

#     -- deprecated --
# ----------------------------------------------------------------------
# -- setPapiNumber($papiNumber) --
#   set the PAPI name of the experiment
# ----------------------------------------------------------------------
#
#sub setPapiNumber {
#    my ($self, $papiNumber) = @_;
#    $self->{profileHeader}->{papiNumber} = $papiNumber;
#}

# ----------------------------------------------------------------------
# -- setMarchFreq($marchFreq) --
#   set the frequency (marching order)
# ----------------------------------------------------------------------

sub setMarchFreq {
    my ($self, $marchFreq) = @_;
    $self->{profileHeader}->{marchFreq} = $marchFreq;
}

# ----------------------------------------------------------------------
# -- setScaleUnit($scaleUnit) --
#   set the unit of marching order
# ----------------------------------------------------------------------

sub setScaleUnit {
    my ($self, $scaleUnit) = @_;
    $self->{profileHeader}->{scaleUnit} = $scaleUnit;
}

# ----------------------------------------------------------------------
# -- setScaleUnitInSample($scaleUnitInSample) --
#   set the number of units in each sample (marchOrder)
# ----------------------------------------------------------------------

sub setScaleUnitInSample {
    my ($self, $scaleUnitInSample) = @_;
    $self->{profileHeader}->{scaleUnitInSample} = $scaleUnitInSample;
}


# ----------------------------------------------------------------------
#
# methods writing XML document
#
# ----------------------------------------------------------------------

# ----------------------------------------------------------------------
#
# PROFILEXML tag formats
#
# ----------------------------------------------------------------------

my $profileFormatTag      = "<PROFILE version=\"%s\">";
my $profileEndTag         = "</PROFILE>";
my $profileHdrStartTag    = "<PROFILEHDR>";
my $profileHdrEndTag      = "</PROFILEHDR>";
my $profileParamsStartTag = "<PROFILEPARAMS>";
my $profileParamsEndTag   = "</PROFILEPARAMS>";
my $targetFormatTag       = "<TARGET name=\"%s\"/>";
my $metricsStartTag       = "<METRICS>";
my $metricsEndTag         = "</METRICS>";
## my $metricFormatTag       = "<METRIC shortName=\"%s\" nativeName=\"%s\" period=\"%s\" displayName=\"%s\" display=\"%s\"/>";
my $metricFormatTag       = "<METRIC shortName=\"%s\" nativeName=\"%s\" period=\"%s\" displayName=\"%s\" />";
my $profileScopeTreeStartTag = "<PROFILESCOPETREE>"; 
my $profileScopeTreeEndTag   = "</PROFILESCOPETREE>";
my $pgmStartTag = "<PGM n=\"%s\">";
my $pgmEndTag   = "</PGM>";
my $fileStartTag = "<F n=\"%s\">";
my $fileEndTag   = "</F>";
my $procStartTag = "<P n=\"%s\">";
my $procEndTag   = "</P>";
my $stmtStartTag = "<S b=\"%s\" id=\"%s\">";
my $stmtEndTag   = "</S>";
my $stmtMetricFormatTag = "<M n=\"%s\" v=\"%s\"/>";

# ----------------------------------------------------------------------
# -- _writeNewLine() --
# ----------------------------------------------------------------------

sub _writeNewLine {
    my ($self) = @_;
    print {$self->{outFileHandle}} "\n";
}

# ----------------------------------------------------------------------
# -- writePROFILEStartTag() --
#   write a start tag for PROFILE element
# ----------------------------------------------------------------------

sub writePROFILEStartTag {
    my ($self) = @_;
    printf {$self->{outFileHandle}} ($profileFormatTag, "3.0");
    $self->_writeNewLine();
}
		     
# ----------------------------------------------------------------------
# -- writePROFILEEndTag() --
#   write an end tag for PROFILE element
# ----------------------------------------------------------------------

sub writePROFILEEndTag {
    my ($self) = @_;
    print {$self->{outFileHandle}} $profileEndTag;
    $self->_writeNewLine();
}
		     
# ----------------------------------------------------------------------
# -- writePROFILEHeader($hdrInfoRef) --
#   writes an PROFILE Header and Params elements
# ----------------------------------------------------------------------

sub writePROFILEHeader {
    my ($self, $hdrInfoRef) = @_;
    my $outFileHandle = $self->{outFileHandle};
    my $header = $self->{profileHeader};
    my $i;
    
    #
    # write a PROFILE HEADER element
    #
    
    # write a start tag for PROFILEHEADER element
    print {$outFileHandle} $profileHdrStartTag;
    $self->_writeNewLine();
    
    # write out the header info
    foreach $line (@$hdrInfoRef) {
	print {$outFileHandle} doEscape($line);
    }

    # write an end tag for PROFILEHEADER element
    print {$outFileHandle} $profileHdrEndTag;
    $self->_writeNewLine();

    #
    # write a PROFILEPARAMS element
    #

    # write a start tag for PROFILEPARAMS element
    print {$outFileHandle} $profileParamsStartTag;
    $self->_writeNewLine();

    # write a TARGET element
    printf {$outFileHandle} ($targetFormatTag,
			     doEscape($header->{targetName}));
    $self->_writeNewLine();

    # write a start tag for METRICS element
    print {$outFileHandle} $metricsStartTag;
    $self->_writeNewLine();
   
    # write a METRIC element
    printf {$outFileHandle} ($metricFormatTag,
			     doEscape($header->{shortName}),
			     doEscape($header->{nativeName}),
			     doEscape($header->{period}),
			     doEscape($header->{displayName}),
## not needed here	     doEscape($header->{display})
			     );
    $self->_writeNewLine();
    
    # write an end tag for METRICS element
    print {$outFileHandle} $metricsEndTag;
    $self->_writeNewLine();

    # write an end tag for PROFILEPARAMS element
    print {$outFileHandle} $profileParamsEndTag;
    $self->_writeNewLine();
}

# ----------------------------------------------------------------------
# -- writeScopeTreeStartTag() --
#   write a start tag for PROFILESCOPETREE element
# ----------------------------------------------------------------------

sub writeScopeTreeStartTag {
    my ($self) = @_;
    print {$self->{outFileHandle}} $profileScopeTreeStartTag;
    $self->_writeNewLine();
}

# ----------------------------------------------------------------------
# -- writeScopeTreeEndTag() --
#   write an end tag for PROFILESCOPETREE element
# ----------------------------------------------------------------------

sub writeScopeTreeEndTag {
    my ($self) = @_;
    print {$self->{outFileHandle}} $profileScopeTreeEndTag;
    $self->_writeNewLine();
}

# ----------------------------------------------------------------------
# -- writeSTPgmStartTag($name) --
#   write a start tag for PGM element
# ----------------------------------------------------------------------

sub writeSTPgmStartTag {
    my ($self, $name) = @_;
    printf {$self->{outFileHandle}} ($pgmStartTag,
				     doEscape($name));
    $self->_writeNewLine();
}

# ----------------------------------------------------------------------
# -- writeSTPgmEndTag() --
#   write an end tag for PGM element
# ----------------------------------------------------------------------

sub writeSTPgmEndTag {
    my ($self) = @_;
    print {$self->{outFileHandle}} $pgmEndTag;
    $self->_writeNewLine();
}

# ----------------------------------------------------------------------
# -- writeSTFileStartTag($name) --
#   write a start tag for F (file) element
# ----------------------------------------------------------------------

sub writeSTFileStartTag {
    my ($self, $name) = @_;
    printf {$self->{outFileHandle}} ($fileStartTag,
				     doEscape($name));
    $self->_writeNewLine();
}

# ----------------------------------------------------------------------
# -- writeSTFileEndTag() --
#   write an end tag for F (file) element
# ----------------------------------------------------------------------

sub writeSTFileEndTag {
    my ($self) = @_;
    print {$self->{outFileHandle}} $fileEndTag;
    $self->_writeNewLine();
}

# ----------------------------------------------------------------------
# -- writeSTProcStartTag($name) --
#   write a start tag for P (proc) element
# ----------------------------------------------------------------------

sub writeSTProcStartTag {
    my ($self, $name) = @_;
    printf {$self->{outFileHandle}} ($procStartTag,
				     doEscape($name));
    $self->_writeNewLine();
}

# ----------------------------------------------------------------------
# -- writeSTProcEndTag() --
#   write an end tag for P (proc) element
# ----------------------------------------------------------------------

sub writeSTProcEndTag {
    my ($self) = @_;
    print {$self->{outFileHandle}} $procEndTag;
    $self->_writeNewLine();
}

# ----------------------------------------------------------------------
# -- writeSTStmtStartTag($startLine, $id) --
#   write a start tag for S (stmt) element
# ----------------------------------------------------------------------

sub writeSTStmtStartTag {
    my ($self, $startLine, $id) = @_;
    printf {$self->{outFileHandle}} ($stmtStartTag, $startLine, $id);
    $self->_writeNewLine();
}

# ----------------------------------------------------------------------
# -- writeSTStmtEndTag() --
#   write an end tag for S (stmt) element
# ----------------------------------------------------------------------

sub writeSTStmtEndTag {
    my ($self) = @_;
    print {$self->{outFileHandle}} $stmtEndTag;
    $self->_writeNewLine();
}

# ----------------------------------------------------------------------
# -- writeSTMetricFormatTag($name) --
#   write a tag for M (metric) element
# ----------------------------------------------------------------------

sub writeSTMetricFormatTag {
    my ($self, $value) = @_;
    printf {$self->{outFileHandle}} ($stmtMetricFormatTag,
				     $self->{profileHeader}->{shortName},
				     $value);
    $self->_writeNewLine();
}

# ----------------------------------------------------------------------
# -- doEscape($content) --
#   return a string with <, >, & and " replaced with &lt;, &gt;, &amp;
#   and &quot;.
# ----------------------------------------------------------------------

$lt = '&lt;';
$leftAngleBracket = "<";
$gt = '&gt;';
$rightAngleBracket = ">";
$amp = '&amp;';
$ampersand = "&";
$quot = '&quot;';
$quoteMark = "\"";

sub doEscape {
    my ($content) = @_;
    $content =~ s/$ampersand/$amp/g;
    $content =~ s/$quoteMark/$quot/g;
    $content =~ s/$leftAngleBracket/$lt/g;
    $content =~ s/$rightAngleBracket/$gt/g;
    $content =~ s/\'/&apos;/g;  # ' ==> &apos;
    return $content;
}

1;
