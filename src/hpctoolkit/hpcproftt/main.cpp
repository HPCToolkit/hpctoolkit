// -*-Mode: C++;-*-
// $Id$

//***************************************************************************
//
// File:
//    $Source$
//
// Purpose:
//    Process hpcrun profile files creating text, html and PROFILE output.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// Author:
//    Written by John Mellor-Crummey and Nathan Tallent, Rice University.
//
//    Adapted from parts of The Visual Profiler by Curtis L. Janssen
//    (cprof.cc).
//
//***************************************************************************

//************************* System Include Files ****************************

#include <fstream>
#include <iomanip>
#include <string>
using std::string;

#include <algorithm>

#include <vector>
using std::vector;

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cmath> // pow

//*************************** User Include Files ****************************

#include <include/general.h>

#include "summary.hpp"

#include <lib/binutils/LM.hpp>
#include <lib/binutils/Seg.hpp>
#include <lib/binutils/Proc.hpp>
#include <lib/binutils/Insn.hpp>
#include <lib/binutils/VMAInterval.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/StrUtil.hpp>

//*************************** Forward Declarations **************************

using namespace std;

//******************************** Variables ********************************

static string the_binary;
static vector<string> the_proffilenms;
static vector<qual_name> annotate; // <load module name, file name>

static bool show_as_html = false;
static bool dump_profile = false;

static bool show_loadmodules = false; // FIXME
static bool show_files = false;
static bool show_funcs = false;
static bool show_lines = false;
static bool show_everything = false;
static bool show_of_force = false;

static bool show_object = false;
static uint64_t show_object_procthresh = 1;

static bool show_as_percent = true;
static int show_collective_thresh = 4;
static string htmldir;

static int debug = 0;

//***************************************************************************

static const char* version_info =
#include <include/HPCToolkitVersionInfo.h>

static const char* usage_details = "\
hpcproftt correlates 'flat' profiles of <executable> with source code\n\
files, procedures, lines or with object code.  It is typically used to\n\
generate plain-text or HTML output.  <hpcrun-file> is a collection of\n\
event-based program counter histograms obtained using hpcrun (or hpcex).\n\
See the man page for a more detailed description of the correlation.\n\
\n\
Options: General\n\
  -d <dir>, --directory <dir>\n\
                      Search <dir> for source files.\n\
  -D <dir>, --recursive-directory <dir>\n\
                      Search <dir> recursively for source files.\n\
  --force             Show data that is not accurate.\n\
  -V, --version       Display the version number.\n\
  -h, --help          Print this message.\n\
  --debug <n>         Debug: use debug level <n>.\n\
\n\
Options: Plain-text and HTML Output\n\
  -e, --everything    Show load-module correlation, file correlation,\n\
                      function correlation, line correlation, and annotated\n\
                      source files.\n\
  -f, --files         Show file correlation.\n\
  -r, --funcs         Show function correlation.\n\
  -l, --lines         Show line correlation.\n\
  -a <file>, --annotate <file> \n\
                      Annotate source file <file>. (Note that <file> must\n\
                      match the string contained in the binary's debugging\n\
                      file tables.)\n\
\n\
  -o, --object        Show object code correlation (plain-text only).\n\
                      Use the -l, --lines option to intermingle source line\n\
                      information with object code.\n\
  --othreshold <n>    Show only object procedures with an event count >= <n>\n\
                      {1}  (Use 0 to see all procedures.)\n\
\n\
  -n, --number        Show number of samples (not percentages).\n\
  -s <n>, --show <n>  Set threshold <n> for showing aggregate data.\n\
  -H <dir>, --html <dir>\n\
                      Output HTML into directory dir.\n\
\n\
Options: PROFILE Output\n\
  -p, --profile       Generate PROFILE output to stdout.\n";

static void
usage(const string &argv0)
{
  cout << "Usage:\n"
       << "  " << argv0 << " [options] <executable> <hpcrun-file>...\n"
       << endl 
       << usage_details;
}

//***************************************************************************

int
real_main(int argc, char* argv[]);

int dump_object(ostream& os, const string& pgm, const vector<ProfFile*>& profs);
int dump_html_or_text(Summary& sum);
int dump_PROFILE(Summary& sum);


int
main(int argc, char* argv[])
{
  try {
    return real_main(argc, argv);
  }
  catch (const Diagnostics::Exception& x) {
    DIAG_EMsg(x.message());
    exit(1);
  } 
  catch (const std::bad_alloc& x) {
    DIAG_EMsg("[std::bad_alloc] " << x.what());
    exit(1);
  } 
  catch (const std::exception& x) {
    DIAG_EMsg("[std::exception] " << x.what());
    exit(1);
  } 
  catch (...) {
    DIAG_EMsg("Unknown exception encountered!");
    exit(2);
  }
}


int
real_main(int argc, char *argv[])
{
  Summary sum;
    
  // -------------------------------------------------------
  // Process arguments
  // -------------------------------------------------------
  for (int i = 1; i < argc; ++i) {
    string arg(argv[i]);

    if (arg == "-h" || arg == "--help") {
      usage(argv[0]);
      return 1;
    }
    else if (arg == "-V" || arg == "--version") {
      cout << argv[0] << ": " << version_info << endl;
      return 0;
    }
    else if (arg == "--debug") {
      if (i<argc-1) {
	i++;
	debug = (int)StrUtil::toLong(argv[i]);
      }
      Diagnostics_SetDiagnosticFilterLevel(debug);
    }

    // 
    else if (arg == "-d" || arg == "--directory") {
      if (i<argc-1) {
	i++;
	sum.add_filesearch(FileSearch(argv[i], false));
      }
    }
    else if (arg == "-D" || arg == "--recursive-directory") {
      if (i<argc-1) {
	i++;
	sum.add_filesearch(FileSearch(argv[i], true));
      }
    }

    //
    else if (arg == "-e" || arg == "--everything") {
      show_everything = true;
    }
    else if (arg == "--force") {
      show_of_force = true;
    }
    else if (arg == "-f" || arg == "--files") {
      show_files = true;
    }
    else if (arg == "-r" || arg == "--funcs") {
      show_funcs = true;
    }
    else if (arg == "-l" || arg == "--lines") {
      show_lines = true;
    }
    else if (arg == "-o" || arg == "--object") {
      show_object = true;
    }
    else if (arg == "--othreshold") {
      if (i<argc-1) {
	i++;
	show_object_procthresh = StrUtil::toUInt64(argv[i]);
      }
    }

    else if (arg == "-n" || arg == "--number") {
      show_as_percent = false;
    }
    else if (arg == "-s" || arg == "--show") {
      if (i<argc-1) {
	i++;
	show_collective_thresh = (int)StrUtil::toLong(argv[i]);
      }
    }
    else if (arg == "-H" || arg == "--html") {
      show_as_html = true;
      if (i<argc-1) {
	i++;
	htmldir = argv[i];
      }
    }
    else if (arg == "-a" || arg == "--annotate") {
      if (i<argc-1) {
	i++;
	annotate.push_back(qual_name_val("", argv[i])); // FIXME
      }
    }

    //
    else if (arg == "-p" || arg == "--profile") {
      dump_profile = true;
    }

    // 
    else if (arg.length() > 0 && arg.substr(0,1) == "-") {
      usage(argv[0]);
      return 1;
    }
    else if (the_binary.size() == 0) {
      the_binary = arg;
    }
    else {
      the_proffilenms.push_back(arg);
    }
  }
  if (the_binary.size() == 0) {
    usage(argv[0]);
    return 1;
  }
  if (the_proffilenms.size() == 0) {
    usage(argv[0]);
    return 1;
  }
  

  // -------------------------------------------------------
  // Process and dump profiling information
  // -------------------------------------------------------

  // 1. Read all profiling data
  if (the_proffilenms.size() == 0) {
    return 1;
  }
  
  vector<ProfFile*> the_profiles(the_proffilenms.size());
  for (unsigned int i = 0; i < the_proffilenms.size(); ++i) {
    try {
      the_profiles[i] = new ProfFile();
      the_profiles[i]->read(the_proffilenms[i]);
    }
    catch (...) {
      DIAG_EMsg("While reading '" << the_proffilenms[i] << "'");
      throw;
    }
    
    if (debug) {
      the_profiles[i]->dump(cout);
    }
  }

  // 2. Dump requested info
  if (show_object) {
    dump_object(std::cout, the_binary, the_profiles);
  }
  else {
    sum.set_debug(debug);
    if (!sum.init(the_binary, the_profiles)) {
      return 1;
    }

    if (dump_profile) {
      dump_PROFILE(sum);
    } 
    else {
      dump_html_or_text(sum);
    }
  }
  
  // 3. Cleanup profiling data
  for (vector<ProfFile*>::iterator it = the_profiles.begin(); 
       it != the_profiles.end(); ++it) {
    delete (*it);
  }
  
  return 0;
}


//***************************************************************************
//
//***************************************************************************

#define XMLAttr(attr)		\
  "=\"" << (attr) << "\""

const char *PROFILEdtd =
#include "lib/xml/PROFILE.dtd.h"

		     const char* I[] = { "", // Indent levels (0 - 5)
    "  ",
    "    ",
    "      ",
    "        ",
    "          " };

static void dump_PROFILE_header(ostream& os);


static void dump_PROFILE_metric(ostream& os, 
				const vector<uint64_t>& countVec,
				const vector<uint64_t>& periodVec,
				int ilevel);

// dump_PROFILE: Given the Summary 'sum', dumps the profile data in
// 'sum' to standard out using the PROFILE format.  Only elementary
// (non-collective) events are dumped.  Note: Metric values are
// reported as number of events: (samples) * (events/sample).
int
dump_PROFILE(Summary& sum)
{
  streambuf *obuf = cout.rdbuf(); 
  ostream os(obuf); // could substitute with an arbitrary ostream.

  // a vector to store periods for use in metric value calculation;
  // indexed by 'shortname'.  period is in events/sample.
  vector<uint64_t> periodVec(sum.ncounter());

  // Make only elementary (non-collective) events visible
  sum.hide_all();
  sum.show_collected();
  
  // --------------------------------------------------------
  // Dump header info
  // --------------------------------------------------------

  dump_PROFILE_header(os);
  os << "<PROFILE version=\"3.0\">\n";
  os << "<PROFILEHDR></PROFILEHDR>\n";
  os << "<PROFILEPARAMS>\n";
  {
    os << I[1] << "<TARGET name" << XMLAttr(the_binary) << "/>\n";
    os << I[1] << "<METRICS>\n";

    for (int i = 0; i < sum.ncounter(); ++i) {
      if (sum.visible(i)) { 
	const Event& ev = sum.event(i);
	periodVec[i] = ev.period();
	const char* units = "PAPI events";
	
	os << I[2] << "<METRIC shortName" << XMLAttr(i);
	os << " nativeName" << XMLAttr(ev.name()); 
	os << " displayName" << XMLAttr(ev.name()); 
	os << " period"     << XMLAttr(ev.period());
	os << " units"      << XMLAttr(units);
	os << "/>\n";
	
	// No need to worry about sum.overflow(i) or sum.outofrange(i) counts.
      }
    }
    os << I[1] << "</METRICS>\n";
    os << "</PROFILEPARAMS>\n";
  }

  // --------------------------------------------------------
  // Iterate through the load module, file and line lists and dump by function
  // --------------------------------------------------------
  
  os << "<PROFILESCOPETREE>\n";
  os << "<PGM n" << XMLAttr(the_binary) << ">\n";
      
  const lmmap_t& lmmap = sum.lmmap();
  lmmap_t::const_iterator h;
  for (h = lmmap.begin(); h != lmmap.end(); ++h) {
    const string& lmname = (*h).first;
    const filemap_t& filemap = (*h).second;

    // Open Load Module
    os << I[1] << "<LM n" << XMLAttr(htmlize(lmname)) << ">\n";
    
    filemap_t::const_iterator i;
    for (i = filemap.begin(); i != filemap.end(); ++i) {
      const string &filename = (*i).first;
      const linemap_t &linemap = (*i).second;
      
      // Open File
      os << I[2] << "<F n" << XMLAttr(htmlize(filename)) << ">\n";
      
      // We want to report statements within procedures/functions.
      // Since files contains lines (approximately statements) and lines
      // contain functions, we have to sift through the information a
      // bit.
      string theFunc = "";
      unsigned int fCount = 0;
      
      linemap_t::const_iterator j;
      for (j = linemap.begin(); j != linemap.end(); ++j) {
	unsigned int lineno = (*j).first;
	const funcmap_t &funcmap = (*j).second;
		
	funcmap_t::const_iterator k;
	for (k = funcmap.begin(); k != funcmap.end(); ++k) {
	  const string &funcname = (*k).first;
	  const vector<uint64_t> countVec = (*k).second;
	  
	  // We are starting a new procedure...
	  if (theFunc != funcname) {
	    theFunc = funcname;
	    
	    // Close prior Procedure if necessary
	    if (fCount != 0) { os << I[3] << "</P>\n"; }
	    
	    // Open a new Procedure
	    os << I[3] << "<P n" << XMLAttr(htmlize(theFunc)) << ">\n";
	    fCount++;
	  }
	  
	  // Dump Statment and Metric info.  A metric value should be
	  // reported as number of events: (samples) * (events/sample).
	  if (lineno > 0) {
	    os << I[4] << "<S b" << XMLAttr(lineno) << ">\n";
	    dump_PROFILE_metric(os, countVec, periodVec, 5);	    
	    os << I[4] << "</S>" << endl;	    
	  } else {
	    // We do not have valid stmt info.
	    dump_PROFILE_metric(os, countVec, periodVec, 4);
	  }
	}
      }

      // Close last Procedure if necessary
      if (fCount != 0) { os << I[3] << "</P>\n"; }
      
      // Close File
      os << I[2] << "</F>\n";
    }
    
    // Close Load Module
    os << I[1] << "</LM>\n";
  }
  
  os << "</PGM>\n";
  os << "</PROFILESCOPETREE>\n";

  // --------------------------------------------------------
  // Dump 'footer'
  // --------------------------------------------------------
  
  os << "</PROFILE>\n"; 
  
  return 0;
}

static void
dump_PROFILE_header(ostream& os)
{
  os << "<?xml version=\"1.0\"?>" << endl;
  os << "<!DOCTYPE PROFILE [\n" << PROFILEdtd << "]>" << endl;
  os.flush();
}

static void 
dump_PROFILE_metric(ostream& os, 
		    const vector<uint64_t>& countVec,
		    const vector<uint64_t>& periodVec,
		    int ilevel)
{
  // Dump metrics with non-zero counts
  for (unsigned int l = 0; l < countVec.size(); ++l) {
    if (countVec[l] > 0) {
      double v = (double)countVec[l] * (double)periodVec[l];
      os << I[ilevel] << "<M n" << XMLAttr(l) << " v" << XMLAttr(v) << "/>\n";
    }
  }
}



//***************************************************************************
// 
//***************************************************************************

void init_html(ostream &outs, const string &desc, const string &exec);
void done_html(ostream &outs);
void add_to_index(ostream &htmlindex, const string &linkname, 
		  const string &filename);

int
dump_html_or_text(Summary& sum)
{
  bool need_newline = false;

  sum.show_only_collective(show_collective_thresh);
  sum.set_display_percent(show_as_percent);

  if (show_as_html) {
    if (mkdir(htmldir.c_str(), 0755)) {
      cout << "ERROR: couldn't open HTML directory \""
	   << htmldir.c_str() << "\": "
	   << strerror(errno)
	   << endl;
      return 1;
    }
    sum.set_html(true);
  }

  streambuf *obuf = cout.rdbuf();
  ostream outs(obuf);

  ofstream htmlindex;
  if (show_as_html) {
    string filename = htmldir + "/index.html";
    htmlindex.open(filename.c_str());
    init_html(htmlindex, "Performance Analysis", the_binary);
    htmlindex << "<ul>" << endl;
  }
  
  if (sum.ncounter() > 0) {
    ofstream outfilestream;
    streambuf *buf;
    if (show_as_html) {
      string subname = "counterdescription.html";
      string filename = htmldir + "/" + subname;
      add_to_index(htmlindex,"Counter Descriptions", subname);
      outfilestream.open(filename.c_str());
      buf = outs.rdbuf(outfilestream.rdbuf());
      init_html(outs,"Counter Description", the_binary);
      outs << "<pre>" << endl;
    }
    if (!show_as_html && need_newline) { 
      outs << endl; need_newline = false; 
    }
    outs << "Columns correspond to the following events"
	 << " [event:period (events/sample)]" << endl;
    for (int i = 0; i < sum.ncounter(); ++i) {
      const Event& e = sum.event(i);
      outs << "  " << e.name() << ":" << e.period()
	   << " - " << sum.event(i).description()
	   << " (" << sum.n_sample(i) << " samples";

      if (sum.overflow(i) != 0)
	outs << " + " << sum.overflow(i) << " overflows";
      if (sum.outofrange(i) != 0)
	outs << " + " << sum.outofrange(i) << " out of range";
      outs << ")";
      if (!sum.visible(i)) outs << " [not shown]";
      outs << endl;
    }
    need_newline = true;
    if (show_as_html) {
      outs << "</pre>" << endl;
      done_html(outs);
      outs.rdbuf(buf);
    }
  }


  if (show_everything || show_loadmodules) {
    if (!show_as_html && need_newline) { 
      outs << endl; need_newline = false; 
    }

    ofstream outfilestream;
    streambuf *buf;
    if (show_as_html) {
      string subname = "loadmodulesummary.html";
      string filename = htmldir + "/" + subname;
      add_to_index(htmlindex, "Load Module Summary", subname);
      outfilestream.open(filename.c_str());
      buf = outs.rdbuf(outfilestream.rdbuf());
      init_html(outs, "Load Module Summary", the_binary/*FIXME*/);
    }
    else outs << "Load Module Summary:" << endl;
    sum.summarize_loadmodules(outs);
    if (show_as_html) {
      done_html(outs);
      outs.rdbuf(buf);
    }
    need_newline = true;
  }

  if (show_everything || show_files) {
    if (!show_as_html && need_newline) { 
      outs << endl; need_newline = false; 
    }
    if (show_of_force || sum.file_info()) {
      ofstream outfilestream;
      streambuf *buf;
      if (show_as_html) {
	string subname = "filesummary.html";
	string filename = htmldir + "/" + subname;
	add_to_index(htmlindex, "File Summary", subname);
	outfilestream.open(filename.c_str());
	buf = outs.rdbuf(outfilestream.rdbuf());
	init_html(outs, "File Summary", the_binary);
      }
      else outs << "File Summary:" << endl;
      sum.summarize_files(outs);
      if (show_as_html) {
	done_html(outs);
	outs.rdbuf(buf);
      }
    }
    else {
      outs << "File Summary skipped because it is not accurate."
	   << endl;
    }
    need_newline = true;
  }

  if (show_everything || show_funcs) {
    if (!show_as_html && need_newline) {
      outs << endl; need_newline = false; 
    }
    if (show_of_force || sum.func_info()) {
      ofstream outfilestream;
      streambuf *buf;
      if (show_as_html) {
	string subname = "funcsummary.html";
	string filename = htmldir + "/" + subname;
	add_to_index(htmlindex,"Function Summary",subname);
	outfilestream.open(filename.c_str());
	buf = outs.rdbuf(outfilestream.rdbuf());
	init_html(outs,"Function Summary", the_binary);
      }
      else outs << "Function Summary:" << endl;
      sum.summarize_funcs(outs);
      if (show_as_html) {
	done_html(outs);
	outs.rdbuf(buf);
      }
    }
    else {
      outs << "Function Summary skipped because it is not accurate."
	   << endl;
    }
    need_newline = true;
  }

  if (show_everything || show_lines) {
    if (!show_as_html && need_newline) { 
      outs << endl; need_newline = false; 
    }
    if (show_of_force || sum.line_info()) {
      ofstream outfilestream;
      streambuf *buf;
      if (show_as_html) {
	string subname = "linesummary.html";
	string filename = htmldir + "/" + subname;
	add_to_index(htmlindex,"Line Summary",subname);
	outfilestream.open(filename.c_str());
	buf = outs.rdbuf(outfilestream.rdbuf());
	init_html(outs,"Line Summary", the_binary);
      }
      else outs << "Line Summary:" << endl;
      sum.summarize_lines(outs);
      if (show_as_html) {
	done_html(outs);
	outs.rdbuf(buf);
      }
    }
    else {
      outs << "Line Summary skipped because it is not accurate."
	   << endl;
    }
    need_newline = true;
  }

  if (show_everything) {
    // put all the files in the annotate list
    annotate.clear();
    namemap n;
    sum.construct_file_namemap(n);
    for (namemap::iterator i = n.begin(); i != n.end(); ++i) {
      annotate.push_back((*i).first);
    }
  }

  if (show_as_html) {
    htmlindex << "</ul>" << endl;
  }

  if (annotate.size()) {
    if (show_of_force || sum.line_info()) {
      if (show_as_html) {
	htmlindex << "<h3>Annotated Source Files</h3>" << endl
		  << "<ul>" << endl;
      }
      for (vector<qual_name>::iterator i = annotate.begin();
	   i != annotate.end(); ++i) {
	const string& filenm = (*i).second;
	if (!sum.file_found(filenm)) { continue; }
	ofstream outfilestream;
	streambuf *buf;
	if (show_as_html) {
	  string hfilenm = htmldir + "/" + sum.html_filename(filenm);
	  outfilestream.open(hfilenm.c_str());
	  buf = outs.rdbuf(outfilestream.rdbuf());
	  init_html(outs, ("Annotation of " + filenm), the_binary); 
	  // FIXME (lm)

	  add_to_index(htmlindex, filenm, sum.html_filename(filenm));
	}
	if (!show_as_html && need_newline) { 
	  outs << endl; need_newline = false; 
	}
	sum.annotate_file(*i, outs);
	need_newline = true;
	if (show_as_html) {
	  done_html(outs);
	  outs.rdbuf(buf);
	}
      }
      if (show_as_html) {
	htmlindex << "</ul>" << endl;
      }
    }
    else {
      if (!show_as_html && need_newline) { 
	outs << endl; need_newline = false; 
      }
      outs << "File Annotations skipped because they are not accurate."
	   << endl;
      need_newline = true;
    }
  }
  outs.rdbuf(0);
  
  if (show_as_html) {
    done_html(htmlindex);
  }
  
  return 0;
}

void
init_html(ostream &outs, const string &desc, const string &exec)
{
  string title = desc + " for " + exec;
  outs << "<html>" << endl;
  outs << "<title>" << endl;
  outs << title << endl;
  outs << "</title>" << endl;
  outs << "<body>" << endl;
  outs << "<h3>" << endl;
  outs << title << endl;
  outs << "</h3>" << endl;
}

void
done_html(ostream &outs)
{
  outs << "</body>" << endl;
  outs << "</html>" << endl;
}

void
add_to_index(ostream &htmlindex, const string &linkname, 
	     const string &filename)
{
  htmlindex << "<li> <a href=\"" << filename << "\">"
            << linkname << "</a>" << endl;
}

//***************************************************************************
// 
//***************************************************************************

class EventCursor {
public:
  EventCursor(const ProfFileLM& proflm, const binutils::LM& lm);
  ~EventCursor();
  
  const vector<const ProfFileEvent*>& eventDescs() const { return m_eventDescs; }
  const vector<uint64_t>& eventTots() const { return m_eventTots; }

  // Assumptions:
  //   - vma's are unrelocated
  //   - over successsive calls, VMA ranges are ascending
  // Result is stored is eventCntAtVMA()
  const vector<uint64_t>& computeEventCountsForVMA(VMA vma) {
    return computeEventCounts(VMAInterval(vma, vma + 1), true);
  }

  // Assumptions:
  //   - same as above
  //   - vmaint: [beg_vma, end_vma), where vmaint is a region
  const vector<uint64_t>& computeEventCounts(const VMAInterval vmaint,
					      bool advanceIndices);

  const vector<uint64_t>& eventCntAtVMA() const { return m_eventCntAtVMA; }

  static bool hasNonZeroEventCnt(const vector<uint64_t>& eventCnt) {
    return hasEventCntGE(eventCnt, 1);
  }

  static bool hasEventCntGE(const vector<uint64_t>& eventCnt, uint64_t val);

private:
  VMA relocate(VMA vma) const {
    VMA ur_vma = vma;
    if (m_doRelocate && vma > m_loadAddr) {
      ur_vma = vma - m_loadAddr;
    }
    return ur_vma;
  }


private:
  bool m_doRelocate;
  VMA m_loadAddr;

  vector<const ProfFileEvent*> m_eventDescs;
  vector<uint64_t> m_eventTots;

  vector<int> m_curEventIdx;
  vector<uint64_t> m_eventCntAtVMA;
};


class ColumnFormatter {
public:
  ColumnFormatter(ostream& os, int num_events, int num_decimals)
    : m_os(os),
      m_num_events(num_events), m_num_decimals(num_decimals),
      m_eventAnnotationWidth(0), 
      m_eventAnnotationWidthTot(0),
      m_scientificFormatThreshold(0) 
  {
    os.setf(ios_base::fmtflags(0), ios_base::floatfield);
    os << std::right << std::noshowpos;

    // Compute annotation width
    if (show_as_percent) {
      if (m_num_decimals >= 1) {
	// xxx.(yy)% or x.xE-yy% (for small numbers)
	m_eventAnnotationWidth = std::max(8, 5 + m_num_decimals);
	m_scientificFormatThreshold = 
	  std::pow((double)10, -(double)m_num_decimals);
      }
      else {
	// xxx%
	m_eventAnnotationWidth = 4;
      }
      os << std::showpoint << std::scientific;
    }
    else {
      // x.xE+yy (for large numbers)
      m_eventAnnotationWidth = 7;
      
      // for floating point numbers over the scientificFormatThreshold
      // printed in scientific format.
      m_scientificFormatThreshold = 
	std::pow((double)10, (double)m_eventAnnotationWidth);
      os << std::setprecision(m_eventAnnotationWidth - 6);
      os << std::scientific;
    }
    os << std::showbase;
    
    m_eventAnnotationWidthTot = ((m_num_events * m_eventAnnotationWidth) 
				 + m_num_events);
  }

  ~ColumnFormatter() { }

  void event_col(uint64_t eventCnt, uint64_t eventTot);

  void fill_cols() {
    m_os << setw(m_eventAnnotationWidthTot) << setfill(' ') << " ";
  }

private:
  ostream& m_os;
  int m_num_events;
  int m_num_decimals;
  int m_eventAnnotationWidth;
  int m_eventAnnotationWidthTot;
  double m_scientificFormatThreshold;
};


void
dump_object_lm(ostream& os, const ProfFileLM& proflm, const binutils::LM& lm);

void
dump_event_summary(ostream& os, 
		   const vector<const ProfFileEvent*>& eventDescs,
		   const vector<uint64_t>& eventCnt,
		   const vector<uint64_t>* eventTot);

int
dump_object(ostream& os, 
	    const string& binary, const vector<ProfFile*>& profiles)
{
  // FIXME
  //   verify that load map is the same for each profile
  //   verify that event list is the same for each load module

  // --------------------------------------------------------
  // 2. For each load module, dump events and object instructions
  // --------------------------------------------------------
  DIAG_Assert(profiles.size() > 0, DIAG_UnexpectedInput);
  
  const ProfFile* prof = profiles[0];
  for (uint i = 0; i < prof->num_load_modules(); ++i) {
    const ProfFileLM& proflm = prof->load_module(i);
    
    // 1. Open and read the load module
    binutils::LM* lm = NULL;
    try {
      lm = new binutils::LM();
      lm->open(proflm.name().c_str());
      lm->read();
    } 
    catch (...) {
      DIAG_EMsg("Exception encountered while reading " << proflm.name());
      throw;
    }
    
    dump_object_lm(os, proflm, *lm);

    delete lm;
  }
  return 0;
}


void
dump_object_lm(ostream& os, const ProfFileLM& proflm, const binutils::LM& lm)
{
  EventCursor eventCursor(proflm, lm);
  ColumnFormatter colFmt(os, eventCursor.eventDescs().size(), 2);

  // --------------------------------------------------------
  // 0. Print event list
  // --------------------------------------------------------

  os << std::setfill('=') << std::setw(77) << "=" << endl;
  os << "Load module: " << proflm.name() << endl;

  const vector<const ProfFileEvent*>& eventDescs = eventCursor.eventDescs();
  const vector<uint64_t>& eventTots = eventCursor.eventTots();
  
  dump_event_summary(os, eventDescs, eventTots, NULL);

  // --------------------------------------------------------
  // 1. Print annotated load module
  // --------------------------------------------------------
  
  for (binutils::LMSegIterator it(lm); it.IsValid(); ++it) {
    binutils::Seg* seg = it.Current();
    if (seg->GetType() != binutils::Seg::Text) { continue; }
    
    // We have a 'TextSeg'.  Iterate over procedures.
    os << endl 
       << "Section: " << seg->name();

    binutils::TextSeg* tseg = dynamic_cast<binutils::TextSeg*>(seg);
    for (binutils::TextSegProcIterator it(*tseg); it.IsValid(); ++it) {
      binutils::Proc* p = it.Current();
      string bestName = GetBestFuncName(p->name());
      
      binutils::Insn* endInsn = p->lastInsn();
      VMAInterval procint(p->begVMA(), p->endVMA() + endInsn->size());
      
      const vector<uint64_t> eventTotsProc = 
	eventCursor.computeEventCounts(procint, false);
      if (!eventCursor.hasEventCntGE(eventTotsProc, show_object_procthresh)) {
	continue;
      }

      // We have a 'Procedure'.  Iterate over instructionsn
      os << endl << endl
	 << "Procedure: " << p->name() << " (" << bestName << ")\n";

      dump_event_summary(os, eventDescs, eventTotsProc, &eventTots);
      os << endl << endl;
      
      string the_file;
      SrcFile::ln the_line = SrcFile::ln_NULL;

      for (binutils::ProcInsnIterator it(*p); it.IsValid(); ++it) {
	binutils::Insn* insn = it.Current();
	VMA vma = insn->vma();
	VMA opVMA = binutils::LM::isa->ConvertVMAToOpVMA(vma, insn->opIndex());

	// 1. Collect metric annotations
	const vector<uint64_t>& eventCntVMA = 
	  eventCursor.computeEventCountsForVMA(opVMA);

	// 2. Print line information (if necessary)
	if (show_lines) {
	  string func, file;
	  SrcFile::ln line;
	  p->GetSourceFileInfo(vma, insn->opIndex(), func, file, line);
	
	  if (file != the_file || line != the_line) {
	    the_file = file;
	    the_line = line;
	    os << the_file << ":" << the_line << endl;
	  }
	}
	
	// 3. Print annotated instruction
	os << hex << opVMA << dec << ": ";
	
	if (eventCursor.hasNonZeroEventCnt(eventCntVMA)) {
	  for (uint i = 0; i < eventCntVMA.size(); ++i) {
	    uint64_t eventCnt = eventCntVMA[i];
	    colFmt.event_col(eventCnt, eventTotsProc[i]);
	  }
	}
	else {
	  colFmt.fill_cols();
	}

	insn->decode(os);
	os << endl;
      }
    }
    os << endl; // section
  }

  os << endl << endl;
}


void
dump_event_summary(ostream& os, 
		   const vector<const ProfFileEvent*>& eventDescs,
		   const vector<uint64_t>& eventCnt,
		   const vector<uint64_t>* eventTot)
{
  os << endl 
     << "Columns correspond to the following events [event:period (events/sample)]\n";

  for (uint i = 0; i < eventDescs.size(); ++i) {
    const ProfFileEvent& profevent = *(eventDescs[i]);
    
    os << "  " << profevent.name() << ":" << profevent.period() 
       << " - " << profevent.description() 
       << " (" << eventCnt[i] << " samples";
    
    if (eventTot) {
      double pct = ((double)eventCnt[i] / (double)(*eventTot)[i]) * 100;
      os << " - " << std::fixed << std::setprecision(4) 
	 << pct << "%";
    }
    
    os << ")" << endl;
  }
}


void
ColumnFormatter::event_col(uint64_t eventCnt, uint64_t eventTot)
{
  if (show_as_percent) {
    double val = 0.0;
    if (eventTot != 0) {
      val = ((double)eventCnt / (double)eventTot) * 100;
    }
    
    m_os << std::setw(m_eventAnnotationWidth - 1);
    if (val != 0.0 && val < m_scientificFormatThreshold) {
      m_os << std::scientific 
	   << std::setprecision(m_eventAnnotationWidth - 7);
    }
    else {
      //m_os.unsetf(ios_base::scientific);
      m_os << std::fixed
	   << std::setprecision(m_num_decimals);
    }
    m_os << std::setfill(' ') << val << "%";
  }
  else {
    m_os << std::setw(m_eventAnnotationWidth);
    if ((double)eventCnt >= m_scientificFormatThreshold) {
      m_os << (double)eventCnt;
    }
    else {
      m_os << std::setfill(' ') << eventCnt;
    }
  }
  m_os << " ";
}



EventCursor::EventCursor(const ProfFileLM& proflm, const binutils::LM& lm)
{
  m_doRelocate = (lm.type() == binutils::LM::SharedLibrary);
  m_loadAddr = (VMA)proflm.load_addr();
  
  // --------------------------------------------------------
  // Find all events for load module and compute totals for each event
  // --------------------------------------------------------
  for (uint i = 0; i < proflm.num_events(); ++i) {
    const ProfFileEvent& profevent = proflm.event(i);
    m_eventDescs.push_back(&profevent);
  }

  m_eventTots.resize(m_eventDescs.size());
  for (uint i = 0; i < m_eventDescs.size(); ++i) {
    const ProfFileEvent& profevent = *(m_eventDescs[i]);
    uint64_t& eventTotal = m_eventTots[i];
    eventTotal = 0;

    for (uint j = 0; j < profevent.num_data(); ++j) {
      const ProfFileEventDatum& evdat = profevent.datum(j);
      uint32_t count = evdat.second;
      eventTotal += count;
    }
  }

  m_curEventIdx.resize(m_eventDescs.size());
  for (uint i = 0; i < m_curEventIdx.size(); ++i) {
    m_curEventIdx[i] = 0;
  }

  m_eventCntAtVMA.resize(m_eventDescs.size());
}


EventCursor::~EventCursor()
{
}


const vector<uint64_t>&
EventCursor::computeEventCounts(const VMAInterval vmaint,
				bool advanceCounters)
{
  // NOTE: An instruction may overlap multiple buckets.  However,
  // because only the bucket corresponding to the beginning of the
  // instruction is charged, we only have to consult one bucket.
  // However, it may be the case that a bucket contains results for
  // more than one instruction.

  for (uint i = 0; i < m_eventCntAtVMA.size(); ++i) {
    m_eventCntAtVMA[i] = 0;
  }

  // For each event, determine if a count exists at vma_beg
  for (uint i = 0; i < m_eventDescs.size(); ++i) {
    const ProfFileEvent& profevent = *(m_eventDescs[i]);
    
    // advance ith bucket until it arrives at vmaint region:
    //   (bucket overlaps beg_vma) || (bucket is beyond beg_vma)
    for (int& j = m_curEventIdx[i]; j < profevent.num_data(); ++j) {
      const ProfFileEventDatum& evdat = profevent.datum(j);
      VMA ev_vma = evdat.first;
      VMA ev_ur_vma = relocate(ev_vma);
      VMA ev_ur_vma_ub = ev_ur_vma + profevent.bucket_size();
      
      if ((ev_ur_vma <= vmaint.beg() && vmaint.beg() < ev_ur_vma_ub) 
	  || (ev_ur_vma > vmaint.beg())) {
	break;
      }
    }
    
    // count until bucket moves beyond vmaint region
    // INVARIANT: upon entry, bucket overlaps or is beyond region
    for (int j = m_curEventIdx[i]; j < profevent.num_data(); ++j) {
      const ProfFileEventDatum& evdat = profevent.datum(j);
      VMA ev_vma = evdat.first;
      VMA ev_ur_vma = relocate(ev_vma);
      VMA ev_ur_vma_ub = ev_ur_vma + profevent.bucket_size();

      if (ev_ur_vma >= vmaint.end()) {
	// bucket is now beyond region
	break;
      }
      else {
	// bucket overlaps region (by INVARIANT)
	uint32_t count = evdat.second;
	m_eventCntAtVMA[i] += count;
	
	// if the vmaint region extends beyond the current bucket,
	// advance bucket
	if (advanceCounters && (vmaint.end() > ev_ur_vma_ub)) {
	  m_curEventIdx[i]++;
	}
      }
    }
  }

  return m_eventCntAtVMA;
}


bool 
EventCursor::hasEventCntGE(const vector<uint64_t>& eventCnt, uint64_t val)
{
  for (uint i = 0; i < eventCnt.size(); ++i) {
    if (eventCnt[i] >= val) {
      return true;
    }
  }
  return false;
}
