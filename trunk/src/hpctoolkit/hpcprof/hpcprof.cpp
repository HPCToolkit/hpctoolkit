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
#include <string>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

//*************************** User Include Files ****************************

#include "hpcprof.h"
#include "summary.h"

//*************************** Forward Declarations **************************

using namespace std;

//******************************** Variables ********************************

static string command;
static vector<string> proffiles;
static vector<qual_name> annotate; // <load module name, file name>

static bool show_as_html = false;
static bool dump_profile = false;

static bool show_loadmodules = false; // FIXME
static bool show_files = false;
static bool show_funcs = false;
static bool show_lines = false;
static bool show_everything = false;
static bool show_of_force = false;

static int show_thresh = 4;
static bool show_as_percent = true;
static string htmldir;

static int debug = 0;

//***************************************************************************

static void
usage(const string &argv0)
{
  cout 
    << "Usage:\n"
    << "  " << argv0 << "    [options] <executable> <hpcrun_file>...\n"
    << "  " << argv0 << " -p [options] <executable> <hpcrun_file>...\n"
    << endl
    << "Options: General\n"
    << "  -h, --help          Print this message.\n"
    << "  --debug             Print debugging info.\n"
    << "  -d, --directory dir Search dir for source files.\n"
    << "  -D, --recursive-directory dir\n"
    << "                      Search dir recursively for source files.\n"
    << "  --force             Show data that is not accurate.\n"
    << "  -v, --version       Display the version number.\n"
    << endl
    << "Options: Text and HTML mode [Default]\n"
    << "  -e, --everything    Show all information.\n"
    << "  -f, --files         Show all files.\n"
    << "  -r, --funcs         Show all functions.\n"
    << "  -l, --lines         Show all lines.\n"
    << "  -a, --annotate file Annotate file.\n"
    << "  -n, --number        Show number of samples (not %).\n"
    << "  -s, --show thres    Set threshold for showing aggregate data.\n"
    << "  -H, --html dir      Output HTML into directory dir.\n"
    << endl
    << "Options: PROFILE mode\n"
    << "  -p, --profile       Dump PROFILE output (for use with HPCToolkit's hpcview).\n"
    << endl;
}

//***************************************************************************

int dump_html_or_text(Summary& sum);
int dump_PROFILE(Summary& sum);

int
main(int argc, char *argv[])
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
      else if (arg == "-n" || arg == "--number") {
          show_as_percent = false;
        }
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
      else if (arg == "-s" || arg == "--show") {
          if (i<argc-1) {
              i++;
              show_thresh = atoi(argv[i]);
            }
        }
      else if (arg == "--debug") {
          debug = 1;
          cout << "Debugging turned on" << endl;
        }
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
      else if (arg == "-l" || arg == "--lines") {
          show_lines = true;
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
      else if (arg == "-v" || arg == "--version") {
          cout << "cprof version " << VPROF_VERSION << endl;
          return 0;
        }
      else if (arg == "-p" || arg == "--profile") {
	  dump_profile = true;
        }      
      else if (arg.length() > 0 && arg.substr(0,1) == "-") {
          usage(argv[0]);
          return 1;
        }
      else if (command.size() == 0) {
          command = arg;
        }
      else {
          proffiles.push_back(arg);
        }
    }
  if (command.size() == 0) {
      usage(argv[0]);
      return 1;
    }
  if (proffiles.size() == 0) {
      usage(argv[0]);
      return 1;
    }
  

  // -------------------------------------------------------
  // Process and dump profiling information
  // -------------------------------------------------------

  sum.set_debug(debug);
  if (!sum.init(command, proffiles)) {
      return 1;
    }

  if (dump_profile) {
      dump_PROFILE(sum);
    } 
  else {
      dump_html_or_text(sum);
    }
  
  return 0;
}

//***************************************************************************

#define XMLAttr(attr) \
 "=\"" << (attr) << "\""

const char *PROFILEdtd =
#include "PROFILE.dtd.h"

const char* I[] = { "", // Indent levels (0 - 5)
		    "  ",
		    "    ",
		    "      ",
		    "        ",
		    "          " };

static void dump_PROFILE_header(ostream& os);


static void dump_PROFILE_metric(ostream& os, 
				const vector<unsigned int>& countVec,
				const vector<ulong>& periodVec,
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
  vector<ulong> periodVec(sum.ncounter());

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
    os << I[1] << "<TARGET name" << XMLAttr(command) << "/>\n";
    os << I[1] << "<METRICS>\n";

    for (int i = 0; i < sum.ncounter(); ++i) {
      if (sum.visible(i)) { 
	const Event& ev = sum.event(i);
	periodVec[i] = ev.period();
	char* units = "PAPI events";
	
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
  os << "<PGM n" << XMLAttr(command) << ">\n";
      
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
	  const vector<unsigned int> countVec = (*k).second;
	  
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
		    const vector<unsigned int>& countVec,
		    const vector<ulong>& periodVec,
		    int ilevel)
{
  // Dump metrics with non-zero counts
  for (unsigned int l = 0; l < countVec.size(); ++l) {
    if (countVec[l] > 0) {
      double v = countVec[l] * periodVec[l];
      os << I[ilevel] << "<M n" << XMLAttr(l) << " v" << XMLAttr(v) << "/>\n";
    }
  }
}



//***************************************************************************

void init_html(ostream &outs, const string &desc, const string &exec);
void done_html(ostream &outs);
void add_to_index(ostream &htmlindex, const string &linkname, 
		  const string &filename);

int
dump_html_or_text(Summary& sum)
{
  bool need_newline = false;

  sum.show_only_collective(show_thresh);
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
      init_html(htmlindex, "Performance Analysis", command);
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
          init_html(outs,"Counter Description", command);
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
	  init_html(outs, "Load Module Summary", command/*FIXME*/);
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
              init_html(outs, "File Summary", command);
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
              init_html(outs,"Function Summary", command);
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
              init_html(outs,"Line Summary", command);
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
                  init_html(outs, ("Annotation of " + filenm), command); 
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
