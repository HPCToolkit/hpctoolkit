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
# Revision 1.2  2004/09/30 19:31:50  eraxxon
# Use FindBin to remove need for PERLLIB environment variable.  Also use it to determine the name of the calling program.
#
# Revision 1.1.1.1  2003/04/21 21:55:04  slindahl
# Initial import of HPCToolkit by Sarah Gonzales as of 4/21/03
#
# Revision 1.5  2002/07/17  18:08:52  eraxxon
# Update copyright notices and RCS strings.
#
# Revision 1.4  2002/04/03  00:35:26  rjf
# fixed filename RE
#
# Revision 1.3  2002/03/26  13:43:01  eraxxon
# Fix error in RE for file names; misc. updates.
#
# Revision 1.3  2000/09/13  22:02:46  dsystem
# hacked in support for prof_hwc style events
#
# Revision 1.2  2000/06/29  00:10:37  rjf
# extended to handle 'ideal' and 'pcsamp' correctly,
# also changed command line args -- rjf
#
# Revision 1.2  2000/05/08 22:09:38  xyuan
# This new version ptranSgi.pm starts to support experiment types other
# than hardware counters.  Currently, the usertime, totaltime, (f)pcsamp and
# (f)pcsampx are supported.  The hooks for ideal, fpe, io and mpi are placed.
#
# Revision 1.1  2000/02/04 15:33:58  xyuan
# Initial revision
#
# ----------------------------------------------------------------------

# ----------------------------------------------------------------------
#
# ptranSgi.pm
#   A module transforming SGI profile output to XML style document
#
#   -- interface --
#
#    -- constructor --
#
#      new($inFileHandle, $outFileHandle)
#        create a new ptranSgi object
#          $inFileHandle           the input file handle (SGI profile output)
#          $outFileHandle          the output file handle (XML document)
#
#    -- methods --
#
#      transform()
#        transform the SGI profile output to XML style document
#
# ----------------------------------------------------------------------

package ptranSgi;

use strict;

use PapiSpec();
use PROFILEXMLWriter();


# ----------------------------------------------------------------------
#
# Experiment types
#   Only the following experiment types are fully supported:
#     totaltime, usertime, (f)pcsamp, (f)pcsampx)
#
# ----------------------------------------------------------------------

my %experimentTypes = (
		       "totaltime"  => 0,
		       "usertime"   => 1,
		       "pcsamp"     => 2,
		       "fpcsamp"    => 3,
		       "pcsampx"    => 4,
		       "fpcsampx"   => 5,
		       "ideal"      => 6,
		       "heap"       => 7,
		       "fpe"        => 8,
		       "io"         => 9,
		       "mpi"        => 10,
		       );


# ----------------------------------------------------------------------
#
# Hardware counter descriptions
#
# ----------------------------------------------------------------------

my %hwcTypes = ( "R10000" => 0, "R12000" => 1, );

my @hardwareCounters =
    (
     ["Cycles", "Issued instructions", "Issued loads",
      "Issued stores", "Issued store conditionals",
      "Failed store conditionals", "Decoded branches",
      "Quadwords written back from secondary cache",
      "Correctable secondary cache data array ECC errors",
      "Primary instruction-cache misses", "Secondary instruction-cache misses",
      "Instruction misprediction from secondary cache way prediction table",
      "External interventions", "External invalidations",
      "Virtual conherency conditions" +
      " (or functional unit completions, depending on hardware version)",
      "Graduated instructions", "Cycles",
      "Graduated loads", "Graduated stores", "Graduated store conditionals",
      "Graduated floating-point instructions",
      "Quadwords written back from primary data cache",
      "TLB misses", "Mispredicted branches", "Primary data cache misses",
      "Secondary data cache misses",
      "Data misprediction from secondary cache way prediction table",
      "External intervention hits in secondary cache",
      "External invalidation hits in secondary cache",
      "Store/prefetch exclusive to clean block in secondary cache",
      "Store/prefetch exclusive to shared block in secondary cache"
      ],
     ["Cycles", "Decoded instructions", "Decoded loads",
      "Decoded stores", "Miss Handling Table occupancy",
      "Failed store conditionals", "Resolved conditional branches",
      "Quadwords written back from secondary cache",
      "Correctable secondary cache data array ECC errors",
      "Primary instruction-cache misses", "Secondary instruction-cache misses",
      "Instruction misprediction from secondary cache way prediction table",
      "External interventions", "External invalidations",
      "ALU/FPU progress cycles", "Graduated instructions",
      "Executed prefetch instructions", "Prefetch primary data cache misses",
      "Graduated loads", "Graduated stores", "Graduated store conditionals",
      "Graduated floating-point instructions",
      "Quadwords written back from primary data cache",
      "TLB misses", "Mispredicted branches", "Primary data cache misses",
      "Secondary data cache misses",
      "Data misprediction from secondary cache way prediction table",
      "State of intervention hits in secondary cache",
      "State of invalidation hits in secondary cache",
      "Store/prefetch exclusive to clean block in secondary cache",
      "Store/prefetch exclusive to shared block in secondary cache"
      ]
     );

# ----------------------------------------------------------------------
#
# Hardware counter experiment types and corresponding PAPI event types
#
# ----------------------------------------------------------------------

my %nativePAPITable =
    (
     "gi_hwc"  => "PAPI_INT_INS",  "fgi_hwc"  => "PAPI_INT_INS",
     "cy_hwc"  => "PAPI_TOT_CYC",  "fcy_hwc"  => "PAPI_TOT_CYC",
     "ic_hwc"  => "PAPI_L1_ICM",   "fic_hwc"  => "PAPI_L1_ICM",
     "isc_hwc" => "PAPI_L2_ICM",   "fisc_hwc" => "PAPI_L2_ICM",
     "dc_hwc"  => "PAPI_L1_DCM",   "fdc_hwc"  => "PAPI_L1_DCM",
     "dsc_hwc" => "PAPI_L2_DCM",   "fdsc_hwc" => "PAPI_L2_DCM",
     "tlb_hwc" => "PAPI_TLB_TL",   "ftlb_hwc" => "PAPI_TLB_TL",
     "gfp_hwc" => "PAPI_FP_INS",   "fgfp_hwc" => "PAPI_FP_INS",
     "fsc_hwc" => "PAPI_CSR_FAL",  "ffsc_hwc" => "PAPI_CSR_FAL",
     "prof_hwc" => "PAPI_PROF",
     );

# ----------------------------------------------------------------------
#
# Regular expressions
#
# ----------------------------------------------------------------------

# regular expressions for the field separator
my $separatorRE      = '^-+';


#
# regular expressions for SpeedShop profile listing header info
#

my $headerStartRE    = '^SpeedShop profile listing';
my $executableNameRE = '^\s*(.*):\s*Target program';
my $experimentNameRE = '^\s*(.*):\s*Experiment name';
my $marchingOrdersRE = '^\s*(.*):cu:\s*Marching orders';
my $hwcMarchingRE    = 'hwc,(\d+),(\d+)';
my $hwctMarchingRE   = 'hwct,(\d+),(\d+),(\d+)';
my $pcSampMarchingRE = 'pc,(\d+),(\d+),(\d+)';
my $utMarchingRE     = 'ut:cu';
my $itMarchingRE     = 'it:cu';
my $ttMarchingRE     = 'ut,(\d+),(\d+),(\d+)';
my $cpuFPURE         = '^\s*(\w+)\s*/\s*(\w+):\s*CPU\s*/\s*FPU';
my $numOfCPUsRE      = '^\s*(\d+):\s*Number of CPUs';
my $clockFrequencyRE = '^\s*(\d+):\s*Clock frequency';

#
# regular expressions for summary of statistics
#
my $statSummStartRE  = '^Summary of ';
my $totalSamplesRE   = '^\s*(\d+):\s*Total samples';

# RE's for hardware counter specific statistics
my $counterNameRE    = '^\s*([\w ]+)\s+\((\d+)\):\s*Counter name \(number\)';
my $overflowValueRE  = '^\s*(\d+):\s*Counter overflow value';
my $totalCountsRE    = '^\s*(\d+):\s*Total counts';

# RE's for usertime and totaltime specific statistics
my $incTracebackSamplesRE = '^\s*(\d+):\s*Samples with incomplete traceback';
my $utAccumedTimeRE  = '^\s*([\d\.]+):\s*Accumulated Time\s+\(secs\.\)';
my $sampleIntervalRE = '^\s*([\d\.]+):\s*Sample interval\s+\(msecs\.\)';

# RE's for pc sampling specific statistics
my $pcAccumedTimeRE  = '^\s*([\d\.]+):\s*Accumulated time\s+\(secs\.\)';
my $timePerSampleRE  = '^\s*([\d\.]+):\s*Time per sample\s+\(msecs\.\)';
my $sampleBinWidthRE = '^\s*([\d\.]+):\s*Samples bin width\s+\(bytes\)';

# RE's for ideal specific statistics
my $totalInstsRE     = '^\s*(\d+):\s*Total number of instructions executed';
my $totalCyclesRE    = '^\s*(\d+):\s*Total computed cycles';
my $execTimeRE    = '^\s*([\d\.]+):\s*Total computed execution time \(secs.\)';
my $avgCPIRE         = '^\s*([\d\.]+):\s*Average cycles / instruction';


#
# regular expressions for function lists
#

my $funcListStartRE  = '^Function list,';
my $funcListEndRE    = '^\s*\d+\s+\d[\d\.]*%\s+\d[\d\.]*%\s+\d+\s+TOTAL';

#
# regular expressions for line lists
#

my $lineListStartRE  = '^Line list\,';
my $numberRE         = '^\s*([\.\d]*)\s+([\.\d]*)\s+([\.\d]*)'
    . '\s+(\d+)\s+(.*)\s+';
my $idealNumberRE    = '^\s*([\d\.]+)\s+([\.\d]+)%\s+([\.\d]+)%'
    . '\s+(\d+)\s+(\d+)\s+(.*)\s+';
my $fpeNumberRE      = '^\s*(\d+)\s+([\d\.]+)\s+([\d\.]+)\s+(.*)\s+';

# old RE
#my $compilationRE    = '\s+\(([^:]+):\s+([\w\.]+),\s+(\d+)(;\s+compiled in\s+([\w\.]+))?\)';
#new
my $compilationRE    = '\s+\((.+):\s+(.+),\s+(\d+)(;\s+compiled in\s+(.+))?\)';



my %fields = (
	      xmlWriter            => undef,
	      inFileHandle         => undef,
	      outFileHandle        => undef,
	      executableName       => undef,
	      experimentName       => undef,
	      experimentType       => undef,
	      cpuType              => undef,
	      fpuType              => undef,
	      numberOfCPUs         => undef,
	      clockFrequency       => undef,
	      totalSamples         => undef,
	      counterNumber        => undef,
	      counterName          => undef,
	      unit                 => undef,
	      counterOverflowValue => undef,
	      totalCounts          => undef,
	      );
    

# ----------------------------------------------------------------------
# -- new($inFileHandle, $outFileHandle) --
# ----------------------------------------------------------------------

sub new {
    my ($class, $inFileHandle, $outFileHandle) = @_;
    my $self = { %fields, };

    # create a new XMLWriter object
    $self->{xmlWriter} = new PROFILEXMLWriter($outFileHandle);
    if (! defined($self->{xmlWriter})) {
	die "cannot allocate a new XMLWriter object\n";
    }

    # record the input and output file handle
    $self->{inFileHandle} = $inFileHandle;
    $self->{outFileHandle} = $outFileHandle;
    
    bless $self, $class;
    return $self;
}

# ----------------------------------------------------------------------
# -- transform() --
#   reads in the profile from $inFileHandle and transforms and writes to
#   $outFileHandle
# ----------------------------------------------------------------------

sub transform {

    my ($self, $expselector) = @_;
    my ($line, $inFileHandle);

    $inFileHandle = $self->{inFileHandle};
    
    # write a start tag for PROFILE element
    $self->{xmlWriter}->writePROFILEStartTag();
    
    # processes the profile
    while ($line = <$inFileHandle>) {

	# process the header information of the profile
	if ($line =~ /$headerStartRE/) {
	    $self->processHeader($line,$expselector);
	}

	# processes the function list of the profile
	if ($line =~ /$funcListStartRE/) {
	    $self->processFuncList($line, $expselector);
	}

	# processes the line list of the profile
	if ($line =~ /$lineListStartRE/) {
	    $self->processLineList($line, $expselector);
	}

    }

    # write an end tag for PROFILE element
    $self->{xmlWriter}->writePROFILEEndTag();

}

# ----------------------------------------------------------------------
# -- processHeader($line) --
#   dump the header into $outFileHandle and extract some experiment
#   infos 
# ----------------------------------------------------------------------

sub processHeader {

    my ($self, $line, $selector) = @_;
    my $headerProcessed = undef;
    my $profileXMLWriter = $self->{xmlWriter};
    my $inFileHandle = $self->{inFileHandle};
    my $outFileHandle = $self->{outFileHandle};
    my @headerLines;
    my $marchingOrders;

    # record the first line which is passed in as $line
    push @headerLines, $line;
    
    # process the rest of the header
    while (<$inFileHandle>) {

	#print {$outFileHandle} $_;
	
	# when all the header is processed, breaks out of while loop
	if (/$separatorRE/ && defined($headerProcessed)) {
	    #print {$outFileHandle} "Header done\n";
	    last;
	}

	# record the header line
	push @headerLines, $_;

	if (/$executableNameRE/) {
	    #print {$outFileHandle} $1, "\n";
	    $self->{executableName} = $1;
	    $profileXMLWriter->setTargetName($1);
	}

	if (/$experimentNameRE/) {
	    my ($papiName, $papiNumber);
	    
	    #print {$outFileHandle} $1, "\n";
	    $papiName = $nativePAPITable{$1};
	    $papiNumber = $PapiSpec::symbolValueHashTable{$papiName};
	    
	    $self->{experimentName} = $1;
	    $self->{experimentType} = $experimentTypes{$1};
	    if (defined($self->{experimentType})
		&& $self->{experimentType} > 6) {
		die "Unsupported experiment type : " .
		    "$self->{experimentName}\n";
	    }
	    $profileXMLWriter->setNativeName($1);
	    $profileXMLWriter->setDisplayName($1);
	    $profileXMLWriter->setDisplay("true");
	    #deprecated: $profileXMLWriter->setPapiName($papiName);
	    #deprecated: $profileXMLWriter->setPapiNumber($papiNumber);
	}

	if (/$marchingOrdersRE/) {
	    #print {$outFileHandle} $1, "\n";
	    $marchingOrders = $1;
	}

	if (/$cpuFPURE/) {
	    #print {$outFileHandle} $1, $2, "\n";
	    $self->{cpuType} = $1;
	    $self->{fpuType} = $2;
	}

	if (/$numOfCPUsRE/) {
	    #print {$outFileHandle} $1, "\n";
	    $self->{numberOfCPUs} = $1;
	}

	if (/$clockFrequencyRE/) {
	    #print {$outFileHandle} $1, "\n";
	    $self->{clockFrequency} = $1;
	}

	if (/$statSummStartRE/) {
	    $self->_processStatSumm(\@headerLines,$selector);
	    $headerProcessed = 1;
	    next;
	}

    }

    # process the marching order
    $self->_processMarchOrder($marchingOrders,$selector);
    $profileXMLWriter->setPeriod($self->{counterOverflowValue});
	    
    # write a PROFILEHDR element
    $profileXMLWriter->writePROFILEHeader(\@headerLines);

}

# ----------------------------------------------------------------------
# -- processFuncList($line) --
#   extracts information from the function list
# ----------------------------------------------------------------------

sub processFuncList {

    my ($self, $line, $selector) = @_;

    # currently a no_op
}
    
# ----------------------------------------------------------------------
# -- processLineList($line) --
#   extracts information from the line list
# ----------------------------------------------------------------------

sub processLineList {

    my ($self, $line, $eselector) = @_;
    my ($currentFunc, $currentLdMod, $currentSrcFile,
	$currentComplUnit, $restOfLine);
    my ($lineNumber, $counts, $samples, $percentage, $funcName,
	$compileUnit, $loadModule, $sourceFile);
    my ($profileXMLWriter, $inFileHandle);
    my ($curFuncCt);
    
    $profileXMLWriter = $self->{xmlWriter};
    $inFileHandle = $self->{inFileHandle};
    
    # start only when pass the separator
    while (<$inFileHandle> !~ /$separatorRE/) {
    }

    # write a start tag for PROGRAMSCOPETREE and PGM elements
    $profileXMLWriter->writeScopeTreeStartTag();
    $profileXMLWriter->writeSTPgmStartTag($self->{executableName});
    $curFuncCt = 0;
    
    # process all line infos
    while (<$inFileHandle>) {

	# break out of the loop when another separator is encountered
	if (/$separatorRE/) {
	    last;
	}

	# match counts, percentage, samples and restOfLine first;
	# then match (dso: file, line; compilation unit) from restOfLine
	# finally get the middle part, which is function name (could be
	# quite bulky)
	if (!defined($self->{experimentType}) ||
	    ( $self->{experimentType} <= 5)) {
		 if (/$numberRE/) {
		     ($counts, $percentage, $samples, $restOfLine) =
			 ($1, $2, $4, $5);
		 }
		 else {
		     next;
		 }
	     }
	elsif ($self->{experimentType} == 6) {

	    if (/$idealNumberRE/) {
		if ($eselector == 0) {
		    # collecting cycles
		    ($counts,  $samples, $restOfLine) =
			($4,  $4, $6);
		} else {
		    # counting instructions
		    ($counts, $samples, $restOfLine) =
			($5,  $5, $6);
		}
		$percentage = 0.0;  # no valid percentage on line
	    } else {next;}
	}

	#  fpe, io, mpi not supported yet

	if ($restOfLine =~ /$compilationRE$/) {
			
	    ($currentLdMod, $currentSrcFile, $lineNumber,
	     $currentComplUnit) = ($1, $2, $3, $5);
			
	    # replace out the compilation unit match
	    $restOfLine =~ s/$compilationRE//;
	    $currentFunc = $restOfLine;
			
	    # if function name changes, begin a new function
	    if ($currentFunc ne $funcName ||
		$currentLdMod ne $loadModule ||
		$currentSrcFile ne $sourceFile ||
		$currentComplUnit ne $compileUnit) {
			    
		$funcName = $currentFunc;
		$loadModule = $currentLdMod;
		$sourceFile = $currentSrcFile;
		$compileUnit = $currentComplUnit;

		# begin a new function
		if ($curFuncCt > 0) { # must close previous func first
		  $profileXMLWriter->writeSTProcEndTag();
		  $profileXMLWriter->writeSTFileEndTag();
		} 
		$profileXMLWriter->writeSTFileStartTag($sourceFile);
		$profileXMLWriter->writeSTProcStartTag($funcName);
		$curFuncCt += 1;
	    }

	    # if second is used as scale unit, calculate the
	    # number of milli seconds
	    # only need to do this for those pc sampling related
	    # experiments
	    if (defined($self->{experimentType})) {
		if ( (0 < $self->{experimentType})
		    && ( $self->{experimentType} < 6)  )
		{
		    $counts = $counts * 1000;
		}
		# no need to scale counts in ideal.
	    }
	    
	    # write out the line info (unused: $samples, $percentage)
	    $profileXMLWriter->writeSTStmtStartTag($lineNumber, 0);
	    $profileXMLWriter->writeSTMetricFormatTag($counts);
	    $profileXMLWriter->writeSTStmtEndTag(); 
	}
    }

    if ($curFuncCt > 0) { # must close last function
      $profileXMLWriter->writeSTProcEndTag();
      $profileXMLWriter->writeSTFileEndTag();
    }
    
    # write an end tag for PROGRAMSCOPETREE and PGM element
    $profileXMLWriter->writeSTPgmEndTag();
    $profileXMLWriter->writeScopeTreeEndTag();
}

# ----------------------------------------------------------------------
# -- _processMarchOrder($marchingOrders) --
#   process the marching order data
# ----------------------------------------------------------------------

sub _processMarchOrder {
    my ($self, $marchingOrders, $eselector) = @_;

    if (!defined($self->{experimentType}) &&
	!defined($nativePAPITable{$self->{experimentName}})) {
	die "Unrecognized experiment type : ", $self->{experimentName}, "\n";
    }

    # hardware counter experiment
    if (!defined($self->{experimentType})) {

	if ($marchingOrders =~ /$hwcMarchingRE/
	    || $marchingOrders =~ /$hwctMarchingRE/) {

	    if (! defined($self->{counterNumber})) {
		$self->{counterNumber} = $1;
	    }
	    else {
		$self->_verifyHwcNumber($1);
	    }
	    
	    if (! defined($self->{counterOverflowValue})) {
		$self->{counterOverflowValue} = $2;
	    }
	    else {
		$self->_verifyMarchingOrder($2);
	    }
	}
	
	$self->{unit} =
	$hardwareCounters[$hwcTypes{$self->{cpuType}}][$self->{counterNumber}];
    }

    # totaltime or usertime or (f)pcsamp or (f)pcsampx
    elsif ($self->{experimentType} >= 0
	   && $self->{experimentType} <= 5) {
	$self->{unit} = "milli seconds";

	if ($marchingOrders =~ /$utMarchingRE/) {
	}
	if ($marchingOrders =~ /$ttMarchingRE/) {
	    $self->_verifyMarchingOrder($1 / 1000);
	}
	if ($marchingOrders =~ /$pcSampMarchingRE/) {
	    $self->_verifyMarchingOrder($2 / 1000);
	}
    }

    # ideal
    elsif ($self->{experimentType} == 6) {  
	$self->{counterOverflowValue} = 1;

	if($marchingOrders=~ /$itMarchingRE/){ }

	if ( $eselector == 0) {
	    $self->{unit} = "cycles";
	    }
	else {
	    $self->{unit} = "instructions";
	}
    }
	
    # heap
    elsif ($self->{experimentType} == 7) {   # do nothing now
    }

    # fpe
    elsif ($self->{experimentType} == 8) {   # do nothing now
    }

    # io
    elsif ($self->{experimentType} == 9) {   # do nothing now
    }

    # mpi
    elsif ($self->{experimentType} == 10) {  # do nothing now
    }
}


# ----------------------------------------------------------------------
# _processStatSumm()
#   process the statistics summary part
# ----------------------------------------------------------------------

sub _processStatSumm {
    my ($self, $headers, $eselector) = @_;
    my $inFileHandle = $self->{inFileHandle};
    
    if (!defined($self->{experimentType}) &&
	!defined($nativePAPITable{$self->{experimentName}})) {
	die "Unrecognized experiment type : ", $self->{experimentName}, "\n";
    }
	
    while (<$inFileHandle>) {

	if (/$separatorRE/) {
	    last;
	}

	push @$headers, $_;

	if (/$totalSamplesRE/) {
	    $self->{totalSamples} = $1;
	    next;
	}
	
	# hardware counter experiment
	if (!defined($self->{experimentType})) {
	    
	    if (/$counterNameRE/) {
		#print {$outFileHandle} $1, $2, "\n";
		$self->{counterName} = $1;
		$self->{counterNumber} = $2;
	    }
	    if (/$overflowValueRE/) {
		#print {$outFileHandle} $1, "\n";
		$self->{counterOverflowValue} = $1;
	    }
	    if (/$totalCountsRE/) {
		#print {$outFileHandle} $1, "\n";
		$self->{totalCounts} = $1;
	    }
	    next;
	}

	# usertime or totaltime
	if ($self->{experimentType} == 0
	    || $self->{experimentType} == 1) {
	    
	    if (/$incTracebackSamplesRE/) {  # do nothing now
	    }
	    if (/$utAccumedTimeRE/) {
		$self->{counterName} = "SGI_timers" ;
		$self->{counterNumber} = 0;
		$self->{totalCounts} = $1;
	    }
	    if (/$sampleIntervalRE/) {
		$self->{counterOverflowValue} = $1;
	    }
	}

	# (f)pcsamp and (f)pcsampx
	if ($self->{experimentType} >= 2
	    && $self->{experimentType} <= 5) {

	    if (/$pcAccumedTimeRE/) {
		$self->{counterName} = "SGI_pcsamp" ;
		$self->{counterNumber} = 0;
		$self->{totalCounts} = $1;
	    }
	    if (/$timePerSampleRE/) {
		$self->{counterOverflowValue} = $1;
	    }
	    if (/$sampleBinWidthRE/) {	     # do nothing now
	    }
	}

	# ideal
	if ($self->{experimentType} == 6) {  

	    if (/$totalInstsRE/) {
		$self->{counterName} = "SGI_ideal" ;
		$self->{counterNumber} = $eselector;
		$self->{counterOverflowValue} = 1;

		if ($eselector == 1) {
		    $self->{totalCounts} = $1;
		}
	    }
	    if (/$totalCyclesRE/) {
		if ($eselector == 0) {
		    $self->{totalCounts} = $1;
		}
	    }

	    if (/$execTimeRE/) {
		# no_op for now
	    }

	    if (/$avgCPIRE/) {
		# no_op for now
	    }


	}
	
	# heap
	if ($self->{experimentType} == 7) {  # do nothing now
	}

	# fpe
	if ($self->{experimentType} == 8) {  # do nothing now
	}

	# io
	if ($self->{experimentType} == 9) {  # do nothing now
	}

	# mpi
	if ($self->{experimentType} == 10) { # do nothing now
	}
    }

    # if time is used, recalculate total counts
    if ($self->{experimentType} >= 0
	&& $self->{experimentType} <= 5) {

	$self->{totalCounts} /= $self->{counterOverflowValue} / 1000;
	#print STDERR $self->{counterOverflowValue}, "\n";
	#print STDERR $self->{totalCounts}, "\n";
    }
}


# ----------------------------------------------------------------------
# _verifyMarchingOrder($marchingOrder)
#   Verify whether the marching order is consistent.
# ----------------------------------------------------------------------

sub _verifyMarchingOrder {
    my ($self, $marchingOrder) = @_;

    if ($self->{counterOverflowValue} != $marchingOrder) {
	die "Marching order information are inconsistent : " .
	    "$marchingOrder vs $self->{counterOverflowValue}\n";
    }
}


# ----------------------------------------------------------------------
# _verifyHwcNumber($hwcNumber)
#   Verify whether the hardware counter number is consistent.
# ----------------------------------------------------------------------

sub _verifyHwcNumber {
    my ($self, $hwcNumber) = @_;

    if ($self->{counterNumber} != $hwcNumber) {
	die "Hardware counter number information are inconsistent : " .
	    "$hwcNumber vs $self->{counterNumber}\n";
    }
}


1;


