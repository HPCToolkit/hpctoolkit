# $Id$

# A sample makefile, illustrating an alternative scripting strategy
# for collecting HPCView input data.
#
# The default target ('all') will collect profile data (using ssrun
# and ptran), generate program structure information (using bloop) and
# then create a hypertext database using hpcview.

# Note from the speedshop man page --
#   Updates to the MPI 3.1 release (MPI 3.1.x.x) reorganize the dynamic
#   objects specified by _RLD_LIST so that they are not in the position
#   ssrun expects them to be.  The workaround is to set environment
#   variable MPI_RLD_HACK_OFF as follows:
#
#          % setenv MPI_RLD_HACK_OFF 1

#############################################################################

# The path to the application (watch out for trailing spaces!)
HPCPATH = .
# The application you want to profile (watch out for trailing spaces!)
HPCAPPL = myprog
# Arguments to the application
HPCARGS = myargs

# The hpcview configuration file to use
HPCVIEW-CFG = hpcview_cfg-sgi-makefile.xml

#############################################################################

# This variable is a string containing the command that
# is to be run by ssrun.
# This works for mpi jobs on a single SGI node, but other
# orderings of mpirun, dplace, and ssrun may be required.
RUNCMD = $(HPCPATH)/$(HPCAPPL) $(HPCARGS)

# This is going to be the root of the profile data file names.
PFROOT = $(HPCAPPL)


all: htmlready

#############################################################################

# Notes: 
#   - In the rules below, we remove profiling data before regenerating it
#     again.
#   - Sentinal files such as 'htmlready' keep track of what's been run.


htmlready: ssruns translate prog_structure
	hpcview -z $(HPCVIEW-CFG)
	echo `date` | cat >htmlready


# Collect the raw profile information
ssruns: fcy_hwc fdc_hwc fdsc_hwc ftlb_hwc fgfp_hwc
	echo "ssruns done"  | cat >ssruns

fcy_hwc:
	/usr/bin/rm -f $(HPCAPPL).fcy_hwc.*
	ssrun -fcy_hwc $(RUNCMD)
	echo `date` | cat >fcy_hwc

fdc_hwc:
	/usr/bin/rm -f $(HPCAPPL).fdc_hwc.*
	ssrun -fdc_hwc  $(RUNCMD)
	echo `date` | cat >fdc_hwc

fdsc_hwc:
	/usr/bin/rm -f $(HPCAPPL).fdsc_hwc.*
	ssrun -fdsc_hwc $(RUNCMD)
	echo `date` | cat >fdsc_hwc

ftlb_hwc:
	/usr/bin/rm -f $(HPCAPPL).ftlb_hwc.*
	ssrun -ftlb_hwc $(RUNCMD)
	echo `date` | cat >ftlb_hwc

fgfp_hwc:
	/usr/bin/rm -f $(HPCAPPL).fgfp_hwc.*
	ssrun -fgfp_hwc $(RUNCMD)
	echo `date`  | cat >fgfp_hwc

# Gather source file information using prof -lines
$(PFROOT).fcy_hwc.prof: fcy_hwc
	prof -lines $(HPCAPPL).fcy_hwc.* > $(PFROOT).fcy_hwc.prof

$(PFROOT).fdc_hwc.prof: fdc_hwc
	prof -lines $(HPCAPPL).fdc_hwc.* > $(PFROOT).fdc_hwc.prof

$(PFROOT).fdsc_hwc.prof: fdsc_hwc
	prof -lines $(HPCAPPL).fdsc_hwc.* > $(PFROOT).fdsc_hwc.prof

$(PFROOT).ftlb_hwc.prof: ftlb_hwc
	prof -lines $(HPCAPPL).ftlb_hwc.* > $(PFROOT).ftlb_hwc.prof

$(PFROOT).fgfp_hwc.prof: fgfp_hwc
	prof -lines $(HPCAPPL).fgfp_hwc.* > $(PFROOT).fgfp_hwc.prof

# Translate the profile information into the standard XML PROFILE format
translate: fcy_hwc.pxml fdc_hwc.pxml fdsc_hwc.pxml ftlb_hwc.pxml fgfp_hwc.pxml
	echo `date` | cat >translate

fcy_hwc.pxml: $(PFROOT).fcy_hwc.prof
	ptran < $(PFROOT).fcy_hwc.prof  >fcy_hwc.pxml

fdc_hwc.pxml: $(PFROOT).fdc_hwc.prof
	ptran < $(PFROOT).fdc_hwc.prof   >fdc_hwc.pxml

fdsc_hwc.pxml: $(PFROOT).fdsc_hwc.prof
	ptran < $(PFROOT).fdsc_hwc.prof  >fdsc_hwc.pxml

ftlb_hwc.pxml: $(PFROOT).ftlb_hwc.prof
	ptran < $(PFROOT).ftlb_hwc.prof  >ftlb_hwc.pxml

fgfp_hwc.pxml: $(PFROOT).fgfp_hwc.prof
	ptran < $(PFROOT).fgfp_hwc.prof  >fgfp_hwc.pxml

# Gather program structure data
prog_structure: 
	bloop $(HPCPATH)/$(HPCAPPL) > bloop.psxml
	echo `date` | cat >prog_structure

