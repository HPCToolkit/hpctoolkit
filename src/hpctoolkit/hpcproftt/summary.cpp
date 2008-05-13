// -*-Mode: C++;-*-
// $Id$

//***************************************************************************
//
// File:
//    $Source$
//
// Purpose:
//    Classes for representing and storing information about profiles.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// Author:
//    Written by John Mellor-Crummey and Nathan Tallent, Rice University.
//
//    Adapted from parts of The Visual Profiler by Curtis L. Janssen
//    (summary.cc).
//
//***************************************************************************

//************************* System Include Files ****************************

#include <fstream>
#include <iomanip>
#include <algorithm>
#include <sstream>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <math.h>

//*************************** User Include Files ****************************

#include "summary.hpp"

#include <lib/isa/ISATypes.hpp>

//*************************** Forward Declarations **************************

using namespace std;

//***************************************************************************

//////////////////////////////////////////////////////////////////////////
// utility functions

std::string
htmlize(const std::string &in)
{
  string ret(in);
  size_t i;
  // replace & with &amp;
  i=0;
  while ((i=ret.find('&',i)) != string::npos) ret.replace(i++,1,"&amp;");
  // replace < with &lt;
  while ((i=ret.find('<')) != string::npos) ret.replace(i,1,"&lt;");
  // replace > with &gt;
  while ((i=ret.find('>')) != string::npos) ret.replace(i,1,"&gt;");
  // replace "" with &quot;
  while ((i=ret.find('"')) != string::npos) ret.replace(i,1,"&quot;");
  return ret;
}

// valid for x >= 0
static inline int
round_to_nearest_int(double x)
{
  return int(x+0.5);
}

// stringcat: a quick (but inefficient) way of building a string from
// numbers and other sorts of things.
template <class T> string 
stringcat(const string& s, T n)
{
  ostringstream oss;
  oss << s << n;
  return oss.str();
} 

///////////////////////////////////////////////////////////////////////
// Event

Event::Event(const char* name, const char* desc, uint64_t period)
{
  name_ = name;
  description_ = desc;
  period_ = period;
}

Event::~Event()
{
}

const char *
Event::name() const
{
  return name_.c_str();
}

const char *
Event::description() const
{
  return description_.c_str();
}

uint64_t
Event::period() const
{
  return period_;
}

///////////////////////////////////////////////////////////////////////
// CollectiveEvent

CollectiveEvent::CollectiveEvent(const Event* e, Op op)
  : Event(e->name(), e->description(), e->period())
{
  op_ = op;
  name_ = Event::name_;
  description_ = Event::description_;
  switch(op) {
  case Sum:
    name_.append(" (sum)");
    description_.append(" (Summed over all events of this type.)");
    break;
  case Min:
    name_.append(" (min)");
    description_.append(" (The minimum for events of this type.)");
    break;
  case Max:
    name_.append(" (max)");
    description_.append(" (The maximum for events of this type.)");
    break;
  default:
    cerr << "CollectiveEvent: internal error: bad op in CTOR"
	 << endl;
    abort();
  };
}

CollectiveEvent::~CollectiveEvent()
{
}

const char *
CollectiveEvent::name() const
{
  return name_.c_str();
}

const char *
CollectiveEvent::description() const
{
  return description_.c_str();
}

///////////////////////////////////////////////////////////////////////
// FileSearch

bool
is_directory(const string &file)
{
  struct stat buf;
  stat(file.c_str(), &buf);
  return S_ISDIR(buf.st_mode);
}

FileSearch::FileSearch(const std::string &directory,
                       bool recursive)
  : directory_(directory), recursive_(recursive)
{
  if (directory_.size() == 0) directory == ".";
}

string
FileSearch::resolve(const string &directory, const string &file) const
{
  if (directory.size() == 0) return file;

  // put the filenames in a set to sort them
  set<string> filenames;
  DIR *dir = opendir(directory.c_str());
  if (dir == 0) return "";
  struct dirent *de;
  while ((de = readdir(dir))) { filenames.insert(de->d_name); }
  closedir(dir);

  for (set<string>::const_iterator i = filenames.begin();
       i != filenames.end();
       ++i) {
    if ((*i) == ".") continue;
    else if ((*i) == "..") continue;
    else if (is_directory(directory + "/" + (*i))) {
      if (recursive_) {
	string subdirfile = resolve(directory + "/" + (*i), file);
	if (subdirfile.size() > 0) return subdirfile;
      }
    }
    else if (file == (*i)) {
      return directory + "/" + (*i);
    }
  }

  return "";
}

string
FileSearch::resolve(const string &file) const
{
  string::size_type slash = file.rfind("/");
  if (slash == string::npos) return resolve(directory_, file);
  string basename = file.substr(slash+1);
  return resolve(directory_, basename);
}

///////////////////////////////////////////////////////////////////////
// utility functions for summary.cc

// this makes sure that uninitialized entries in the namemap
// have the count vector correctly sized and zeroed.
static loccount &
get_namemap_entry(const qual_name& name, namemap &l, int ncounter)
{
  namemap::iterator i = l.find(name);
  if (i == l.end()) {
    i = l.insert(namemap_val(name, loccount())).first;
    loccount &p = (*i).second;
    p.first.resize(ncounter);
    fill(p.first.begin(), p.first.end(), 0);
  }
  return (*i).second;
}

static void
sum_vectors(vector<uint64_t>& v1, const vector<uint64_t>& v2)
{
  for (uint i = 0; i < v1.size() && i < v2.size(); ++i) {
    v1[i] += v2[i];
  }
}

///////////////////////////////////////////////////////////////////////

Summary::Summary(int d)
  : debug_(d)
{
  cleanup();
}


Summary::Summary(const LoadModule *exec, const vector<Prof::Flat::Profile*>& profs, int d)
  : debug_(d)
{
  abort();  // FIXME: do we want a LoadModule?
 
  ctor_init();
  cleanup();
  //FIXME init(exec, profs);
}


void
Summary::ctor_init()
{
  display_percent_ = true;
}


Summary::~Summary()
{
  for (vector<Event*>::iterator i = event_.begin(); i != event_.end(); ++i) {
    delete *i;
  }
}


// 'profs' contains at least one Prof::Flat::Profile
bool
Summary::init(const string& pgm, const vector<Prof::Flat::Profile*>& profs)
{ 
  // FIXME: For what we want to do, the order in which vprof creates a
  // summary is annoying.  For now, to avoid a rewrite, I have hacked
  // and added comments.
  
  pgm_name_ = pgm; // FIXME: make sure we can find the main program (?)
  
  // 1a. Sanity check file modification times
  for (uint i = 0; i < profs.size(); ++i) {
    const Prof::Flat::Profile* prof = profs[i];

    if (i == 0) { 
      prof_mtime_ = prof->mtime();
    } else if (prof->mtime() < prof_mtime_) {
      prof_mtime_ = prof->mtime();
    }
  }

  exec_mtime_ = prof_mtime_; // FIXME: oldest prof (newer than) newest lm

  // 1b. Count basic and aggregate events. (Events are created later.)
  // Create the [event name -> CollectiveLocations] map, which counts
  // collective counters as a side effect.  Note that there is one
  // counter for each event.
  n_event_ = n_collective_ = 0;  
  uint ev_i = 0; // nth event

  for (uint i = 0; i < profs.size(); ++i) {
    const Prof::Flat::Profile* prof = profs[i];
      
    // We inspect only the first load module of each prof file because
    // while one prof file contains multiple load modules, each load
    // module contains the same events.  
    Prof::Flat::Profile::const_iterator it = prof->begin();
    const Prof::Flat::LM* proflm = it->second;
    n_event_ += proflm->num_events();
    for (uint k = 0; k < proflm->num_events(); ++k, ++ev_i) {
      const Prof::Flat::Event& profevent = proflm->event(k);
      string name = stringcat(profevent.name(), profevent.period());
      event2locs_map_[name].set_offset(ev_i, profevent.name(),
				       /*profevent.event()*/
				       n_collective_);
    }
  }
  
  // 1c. Adjust counter indices to reflect total event count
  for (map<string, CollectiveLocations>::iterator i = event2locs_map_.begin();
       i != event2locs_map_.end(); ++i) {
    (*i).second.inc_offset(n_event_);
  }
  
  n_event_ += n_collective_; // simple + aggegate events
  
  // 2. Set vector sizes and initialize vectors
  event_.resize(n_event_);  
  n_sample_.resize(n_event_);
  outofrange_.resize(n_event_);
  overflow_.resize(n_event_);
  n_missing_file_.resize(n_event_);
  n_missing_func_.resize(n_event_);
  visible_.resize(n_event_);
  elementary_.resize(n_event_);

  fill(event_.begin(), event_.end(), (Event*)NULL);
  fill(n_sample_.begin(), n_sample_.end(), 0);
  fill(outofrange_.begin(), outofrange_.end(), 0);
  fill(overflow_.begin(), overflow_.end(), 0);
  fill(n_missing_file_.begin(), n_missing_file_.end(), 0);
  fill(n_missing_func_.begin(), n_missing_func_.end(), 0);
  fill(visible_.begin(), visible_.end(), true);
  fill(elementary_.begin(), elementary_.end(), true);

  // 3. Create non-collective events and process profile information
  // for every load module

  //    FIXME: if there are multiple profile files, some of these
  //    load modules may be the same.  it would be nice not to open
  //    them more than once.
  ev_i = 0;
  for (uint i = 0; i < profs.size(); ++i) {
    const Prof::Flat::Profile* prof = profs[i];

    Prof::Flat::Profile::const_iterator it0 = prof->begin();
    const Prof::Flat::LM* proflm0 = it0->second;

    for (Prof::Flat::Profile::const_iterator it = prof->begin();
	 it != prof->end(); ++it) {
      const Prof::Flat::LM* proflm = it->second;
      process_lm(*proflm, ev_i);
    }

    ev_i += proflm0->num_events();
  }

  // 4. Create collective events (multiple elementary events of the
  // same type and with the same period).
  ev_i = n_event_ - n_collective_; // index of the first collective event
  for (map<string, CollectiveLocations>::iterator k = event2locs_map_.begin();
       k != event2locs_map_.end(); ++k) {
    const CollectiveLocations &loc = (*k).second;
      
    if (loc.nevents() > 1) {
      Event* ev = event_[loc.elementary_location(0)];
      int imin = ev_i++;
      int imax = ev_i++;
      int isum = ev_i++;
      event_[imin] = new CollectiveEvent(ev, CollectiveEvent::Min);
      event_[imax] = new CollectiveEvent(ev, CollectiveEvent::Max);
      event_[isum] = new CollectiveEvent(ev, CollectiveEvent::Sum);
      elementary_[imin] = false;
      elementary_[imax] = false;
      elementary_[isum] = false;

      for (lmmap_t::iterator ilm = lmmap_.begin();
	   ilm != lmmap_.end(); ++ilm) {
	filemap_t& filemap = (*ilm).second;

	for (filemap_t::iterator ifile = filemap.begin();
	     ifile != filemap.end(); ++ifile) {
	  linemap_t &linemap = (*ifile).second;

	  for (linemap_t::iterator iline = linemap.begin();
	       iline != linemap.end(); ++iline) {
	    funcmap_t &funcmap = (*iline).second;

	    for (funcmap_t::iterator ifunc = funcmap.begin();
		 ifunc != funcmap.end(); ++ifunc) {
	      vector<uint64_t> &c = (*ifunc).second;
	      uint64_t value = c[loc.elementary_location(0)];
	      uint64_t vmin = value;
	      uint64_t vmax = value;
	      uint64_t vsum = value;
	      for (int l = 1; l < loc.nevents(); ++l) {
		value = c[loc.elementary_location(l)];
		if (value > vmax) vmax = value;
		if (value < vmin) vmin = value;
		vsum += value;
	      }
	      c[imin] = vmin;
	      c[imax] = vmax;
	      c[isum] = vsum;
	      n_sample_[imin] += vmin;
	      n_sample_[imax] += vmax;
	      n_sample_[isum] += vsum;
	      //cout << "file = " << (*ifile).first
	      //     << " line = " << (*iline).first
	      //     << " func = " << (*ifunc).first;
	      //for (int l=0; l<c.size(); ++l) cout << " " << c[l];
	      //cout << endl;
	    }
	  }
	}
      }
    }
  }

  return true;
}

void
Summary::cleanup()
{
  n_event_ = n_collective_ = 0;  
  prof_mtime_ = exec_mtime_ = (time_t)-1;

  lmmap_.clear();
  event2locs_map_.clear();
  resolve_saved_.clear();

  html_ = false;
  line_info_ = true; // FIXME functionally useless
  file_info_ = true;
  func_info_ = true;
}

bool
Summary::process_lm(const Prof::Flat::LM& proflm, int ev_i_start)
{
  const string& lmname = proflm.name();

  // 1. Open load module
  LoadModule *lm = new BFDLoadModule(lmname); 
  if (lm == 0) { // FIXME
    cerr << "No known way to process load module " << lmname << endl;
    return false;
  }
  lm->set_debug(debug_);

  if (debug_) {
    cerr << "--- Summary::process_lm: '" << lmname << "' (@ " 
	 << hex << proflm.load_addr() << dec << ") ---" << endl;
  }

#if 0 // FIXME: add module qualification; this is mostly useless
  line_info_ = lm->line_info();
  file_info_ = lm->file_info();
  func_info_ = lm->func_info();
#endif

  // 2. Create event and process profile data and find symbolic information
  uint ev_i = ev_i_start; // the nth event

  for (uint l = 0; l < proflm.num_events(); ++l, ++ev_i) {

    // Create event
    const Prof::Flat::Event& profevent = proflm.event(l);
    if (event_[ev_i] == NULL) {
      event_[ev_i] = new Event(profevent.name(), profevent.description(),
			       profevent.period());
    }
    outofrange_[ev_i] += profevent.outofrange();
    overflow_[ev_i] += profevent.overflow();
	  
    // Inspect the event data and try to find symbolic info
    for (uint m = 0; m < profevent.num_data(); ++m) {
      const Prof::Flat::Datum& dat = profevent.datum(m);
      VMA pc = dat.first;
      uint32_t count = dat.second;

      n_sample_[ev_i] += count;

      if (debug_) {
	cerr << "  { samples: addr = " << hex << pc << dec
	     << ", count = " << count << " }" << endl;
      }
	  
      // Try to find symbolic info
      const char *c_funcname, *c_filename;
      uint lineno;
	  
      VMA pc1 = pc;
      if (lm->type() == LoadModule::DSO && pc > proflm.load_addr()) {
	pc1 = pc - proflm.load_addr(); // adjust lookup pc for DSOs
	if (debug_) {
	  cerr << "Adjusting DSO pc_=" << hex << pc << ", load_addr: " 
	       << proflm.load_addr() << " == pc1: " << pc1 
	       << dec << endl;
	}
      }
	      
      bool fnd = lm->find(pc1, &c_filename, &lineno, &c_funcname);
      if (fnd) {
	if (debug_) {
	  cerr << "  { *found @ " << hex << pc1 << dec << ": " 
	       << c_funcname << "\n    " 
	       << c_filename << ":" << lineno << " }" << endl;
	}
      } 
      else {
	// force attribution to an unknown file:function
	c_filename = c_funcname = NULL;
      }

      string funcname, filename;
      if (c_funcname) { funcname = lm->demangle(c_funcname); }
      else { funcname = "<unknown>"; n_missing_func_[ev_i] += count; }

      if (c_filename) { filename = resolve(c_filename); }
      else { filename = "<unknown>"; n_missing_file_[ev_i] += count; }
	  
      // Record symbolic info
      filemap_t& filemap = lmmap_[lmname];
      funcmap_t &funcmap = filemap[filename][lineno];
      if (funcmap.find(funcname) == funcmap.end()) {
	vector<uint64_t> &c = funcmap[funcname];
	c.resize(ncounter());
	fill(c.begin(), c.end(), 0);
	c[ev_i] = count;
      } else {
	funcmap[funcname][ev_i] += count;
      }
    }
  }

  // Cleanup
  delete lm;
  
  return true;
}

int
Summary::nvisible() const
{
  int r=0;
  for (int i = 0; i < ncounter(); ++i) {
    if (visible(i)) r++;
  }
  return r;
}

void
Summary::hide_all()
{
  for (int i = 0; i < ncounter(); ++i) {
    visible_[i] = false;
  }
}

void
Summary::hide_collective()
{
  for (int i = 0; i < ncounter(); ++i) {
    if (!elementary_[i]) visible_[i] = false;
  }
}

void
Summary::hide_collected()
{
  for (int i = 0; i < ncounter(); ++i) {
    if (elementary_[i]) visible_[i] = false;
  }
}

void
Summary::show_collective()
{
  for (int i = 0; i < ncounter(); ++i) {
    if (!elementary_[i]) visible_[i] = true;
  }
}

void
Summary::show_collected()
{
  for (int i = 0; i < ncounter(); ++i) {
    if (elementary_[i]) visible_[i] = true;
  }
}

void
Summary::show_only_collective(int threshold)
{
  fill(visible_.begin(), visible_.end(), true);
  for (map<string,CollectiveLocations>::iterator i = event2locs_map_.begin();
       i != event2locs_map_.end(); ++i) {
    CollectiveLocations &cl = (*i).second;
    if (cl.nevents() < 2) continue;
    bool collective = true, collected = false;
    if (cl.nevents() < threshold) {
      collective = false;
      collected = true;
    }
    for (int j = 0; j < cl.nevents(); ++j)
      visible_[cl.elementary_location(j)] = collected;
    visible_[cl.lmin()] = collective;
    visible_[cl.lmax()] = collective;
    visible_[cl.lsum()] = collective;
  }
}


void
Summary::construct_loadmodule_namemap(namemap &l)
{
  l.clear();

  lmmap_t::iterator h;
  for (h = lmmap_.begin(); h != lmmap_.end(); ++h) {
    const string& lmname = (*h).first;
    qual_name qname = qual_name_val(lmname, "");
    loccount &loci = get_namemap_entry(qname, l, ncounter());
    filemap_t& filemap = (*h).second;
    filemap_t::iterator i;
    for (i = filemap.begin(); i != filemap.end(); ++i) {
      const string& filename = (*i).first;
      loci.second.insert(make_pair(filename, 0));
      linemap_t &linemap = (*i).second;
      linemap_t::iterator j;
      for (j = linemap.begin(); j != linemap.end(); ++j) {
	funcmap_t &funcmap = (*j).second;
	funcmap_t::iterator k;
	for (k = funcmap.begin(); k != funcmap.end(); ++k) {
	  sum_vectors(loci.first, (*k).second);
	}
      }
    }
  }
}

void
Summary::construct_file_namemap(namemap &l)
{
  l.clear();

  lmmap_t::iterator h;
  for (h = lmmap_.begin(); h != lmmap_.end(); ++h) {
    const string& lmname = (*h).first;
    filemap_t& filemap = (*h).second;
    filemap_t::iterator i;
    for (i = filemap.begin(); i != filemap.end(); ++i) {
      const string& filename = (*i).first;
      qual_name qname = qual_name_val(lmname, filename);
      loccount &loci = get_namemap_entry(qname, l, ncounter());
      loci.second.insert(make_pair(filename, 0));
      linemap_t &linemap = (*i).second;
      linemap_t::iterator j;
      for (j = linemap.begin(); j != linemap.end(); ++j) {
	funcmap_t &funcmap = (*j).second;
	funcmap_t::iterator k;
	for (k = funcmap.begin(); k != funcmap.end(); ++k) {
	  sum_vectors(loci.first, (*k).second);
	}
      }
    }
  }
}

void
Summary::construct_line_namemap(namemap &l)
{
  l.clear();

  lmmap_t::iterator h;
  for (h = lmmap_.begin(); h != lmmap_.end(); ++h) {
    const string& lmname = (*h).first;
    filemap_t& filemap = (*h).second;
    filemap_t::iterator i;
    for (i = filemap.begin(); i != filemap.end(); ++i) {
      const string& filename = (*i).first;
      linemap_t &linemap = (*i).second;
      linemap_t::iterator j;
      for (j = linemap.begin(); j != linemap.end(); ++j) {
	uint lineno = (*j).first;
	string fileline = stringcat(filename + ":", lineno);
	qual_name qname = qual_name_val(lmname, fileline);

	loccount &loci = get_namemap_entry(qname, l, ncounter());
	loci.second.insert(make_pair(filename, lineno));
	funcmap_t &funcmap = (*j).second;
	funcmap_t::iterator k;
	for (k = funcmap.begin(); k != funcmap.end(); ++k) {
	  sum_vectors(loci.first, (*k).second);
	}
      }
    }
  }
}

void
Summary::construct_func_namemap(namemap &l)
{
  l.clear();

  lmmap_t::iterator h;
  for (h = lmmap_.begin(); h != lmmap_.end(); ++h) {
    const string& lmname = (*h).first;
    filemap_t& filemap = (*h).second;
    filemap_t::iterator i;
    for (i = filemap.begin(); i != filemap.end(); ++i) {
      const string &filename = (*i).first;
      linemap_t &linemap = (*i).second;
      linemap_t::iterator j;
      for (j = linemap.begin(); j != linemap.end(); ++j) {
	uint lineno = (*j).first;
	funcmap_t &funcmap = (*j).second;
	funcmap_t::iterator k;
	for (k = funcmap.begin(); k != funcmap.end(); ++k) {
	  const string &funcname = (*k).first;
	  qual_name qname = qual_name_val(lmname, funcname);

	  loccount &loci = get_namemap_entry(qname, l, ncounter());
#if 0 // this only gets one location for each function
	  // This creates only a single location entry for the
	  // function.  In fact there may be several in several
	  // files due to inlining.  I'm not necessarily finding
	  // the most sensible function here, unless there is
	  // no inlining, in which case my choice is OK.
	  if (loci.second.begin() == loci.second.end()) {
	    loci.second.insert(make_pair(filename, lineno));
	  }
	  else {
	    uint oldlineno;
	    if ((*loci.second.begin()).first == filename
		&& ((oldlineno = (*loci.second.begin()).second) 
		    > lineno)) {
	      loci.second.insert(make_pair(filename, lineno));
	      loci.second.erase(make_pair(filename, oldlineno));
	    }
	  }
#else // this inserts all locations for each function
	  loci.second.insert(make_pair(filename, lineno));
#endif
	  sum_vectors(loci.first, (*k).second);
	}
      }
    }
  }
}

static bool
file_exists(const std::string &filename)
{
  ifstream s(filename.c_str());
  if (s.good()) return true;
  return false;
}

std::string
Summary::find_file(const std::string &filename) const
{
  string resolved_filename = resolve(filename);
  if (file_exists(resolved_filename)) return resolved_filename;
  return "";
}

bool
Summary::file_found(const std::string &filename) const
{
  return find_file(filename).size() > 0;
}

bool
Summary::annotate_file(const qual_name& qname, ostream &as) const
{
  const string& lmname = qname.first;
  const string& filename = qname.second;

  string resolved_filename = resolve(filename);
  ifstream s(resolved_filename.c_str());

  if (html()) { as << "<pre>" << endl; }
  else {
    as << "File <<" << lmname << ">>" << resolved_filename 
       << " with profile annotations." << endl;
  }

  // FIXME: do a find on the lmname
  bool found = false, ret;
  lmmap_t::const_iterator h;
  for (h = lmmap_.begin(); h != lmmap_.end(); ++h) {
    const filemap_t& filemap = (*h).second;
    filemap_t::const_iterator i = filemap.find(resolved_filename);
    if (i != filemap.end()) {
      ret = annotate_file(s, as, (*i).second);
      found = true;
      break;
    } 
  }

  if (!found) {
    linemap_t nullmap;
    ret = annotate_file(s, as, nullmap);
  }

  if (html()) { as << "</pre>" << endl; }

  return ret;
}

bool
Summary::annotate_file(istream &s, ostream &as, const linemap_t &linemap) const
{
  const int ndecimal = 1;
  int decimalfactor = 1;
  for (int i = 0; i < ndecimal; ++i) decimalfactor *= 10;

  if (!s.good()) {
    as << "Could not read file." << endl;
    return false;
  }
  vector<uint64_t> counters(ncounter());
  int width;
  if (display_percent()) {
    if (ndecimal) width = 5 + ndecimal;
    else width = 4;
  }
  else { width = 5; }

  int i;
  int lineno = 1;
  while (s.good()) {
    string line;
    getline(s,line);
    linemap_t::const_iterator iline = linemap.find(lineno);
    if (html()) as << "<a name=\"" << lineno << "\">";
    as << setw(width) << lineno;
    if (html()) as << "</a>";
    if (iline != linemap.end()) {
      fill(counters.begin(), counters.end(), 0);
      const funcmap_t &funcmap = (*iline).second;
      funcmap_t::const_iterator ifunc;
      for (ifunc = funcmap.begin(); ifunc != funcmap.end(); ++ifunc) {
	sum_vectors(counters, (*ifunc).second);
      }
      for (i = 0; i < ncounter(); ++i) {
	if (visible(i)) {
	  if (display_percent() && n_sample(i)) {
	    int value = round_to_nearest_int(decimalfactor*100.0
					     *counters[i]/n_sample(i));
	    as << ' '
	       << setw(width-1-(ndecimal?ndecimal+1:0))
	       << value/decimalfactor;
	    if (ndecimal) {
	      char oldfill = as.fill('0');
	      as << '.' << setw(ndecimal) << value%decimalfactor;
	      as.fill(oldfill);
	    }
	    as << '%';
	  }
	  else {
	    as << " " << setw(width)
	       << counters[i];
	  }
	}
      }
    }
    else {
      for (i = 0; i < ncounter(); ++i) {
	if (visible(i)) as << " " << setw(width) << " ";
      }
    }
    as << " " << (html() ? htmlize(line) : line) << endl;
    lineno++;
  }

  return true;
}

void
Summary::print(ostream& o) const
{
  o << "n_sample = [";
  int ic;
  for (ic = 0; ic < ncounter(); ++ic) {
    if (ic) o << " ";
    o << n_sample_[ic];
  }
  o << "]" << endl;

  lmmap_t::const_iterator h;
  for (h = lmmap_.begin(); h != lmmap_.end(); ++h) {
    const string& lmname = (*h).first;
    const filemap_t& filemap = (*h).second;
    filemap_t::const_iterator i;
    for (i = filemap.begin(); i != filemap.end(); ++i) {
      const string &filename = (*i).first;
      o << "  [" << lmname << "]" << filename << endl;
      const linemap_t &linemap = (*i).second;
      linemap_t::const_iterator j;
      for (j = linemap.begin(); j != linemap.end(); ++j) {
	uint lineno = (*j).first;
	const funcmap_t &funcmap = (*j).second;
	funcmap_t::const_iterator k;
	for (k = funcmap.begin(); k != funcmap.end(); ++k) {
	  const string &funcname = (*k).first;
	  const vector<uint64_t> &counts = (*k).second;
	  o << "    " << lineno << " " << funcname << " [";
	  for (uint l = 0; l < counts.size(); ++l) {
	    if (l) o << " ";
	    o << counts[l];
	  }
	  o << "]" << endl;
	}
      }
    }
  }

  for (h = lmmap_.begin(); h != lmmap_.end(); ++h) {
    const string& lmname = (*h).first;
    const filemap_t& filemap = (*h).second;
    filemap_t::const_iterator i;
    for (i = filemap.begin(); i != filemap.end(); ++i) {
      const string& filename = (*i).first;
      annotate_file(qual_name_val(lmname, filename), cout);
    }
  }
}

void
Summary::summarize_loadmodules(ostream &o)
{
  summarize_generic(&Summary::construct_loadmodule_namemap, o);
}

void
Summary::summarize_files(ostream &o)
{
  summarize_generic(&Summary::construct_file_namemap, o);
}

void
Summary::summarize_lines(ostream &o)
{
  summarize_generic(&Summary::construct_line_namemap, o);
}

void
Summary::summarize_funcs(ostream &o)
{
  summarize_generic(&Summary::construct_func_namemap, o);
}

typedef std::set<std::pair<uint, const qual_name*>,
                 greater<std::pair<uint, const qual_name*> > > setpair;

void
Summary::summarize_generic(void (Summary::*construct)(namemap&),
                           ostream &o)
{
  if (html()) o << "<pre>" << endl;

  namemap names;
  (this->*construct)(names);

  // Determine the width needed to annotate margin with counts.
  uint j;
  uint maxcount = 0;
  for (namemap::iterator i = names.begin(); i != names.end(); ++i) {
    vector<uint64_t> &counts = (*i).second.first;
    for (j = 0; j < counts.size(); ++j) {
      if (counts[j] > maxcount) { maxcount = counts[j]; }
    }
  }
  int widthcounts;
  if (display_percent()) { 
    widthcounts = 3; 
  }
  else {
    widthcounts = 1;
    while (maxcount /= 10) widthcounts++;
  }

  // If there are multiple event annotations, the summary can be
  // sorted according to one of them.  Collect all names into a set
  // that will sort them according to the chosen event.
  uint sorting_index = ncounter();
  setpair sortingset;
  for (namemap::iterator i = names.begin(); i != names.end(); ++i) {
    const qual_name& qname = (*i).first;
      
    vector<uint64_t> &counts = (*i).second.first;
    uint count = 0;
    if (sorting_index && sorting_index-1 < counts.size()) {
      count = counts[sorting_index-1];
    }
    sortingset.insert(make_pair(count, &qname));
  }

  const int ndecimal = 1;
  int decimalfactor = 1;
  for (int i = 0; i < ndecimal; ++i) decimalfactor *= 10;

  // Output the summary
  for (setpair::iterator i = sortingset.begin(); i != sortingset.end(); ++i) {
    const qual_name& qname = *((*i).second);
    const loccount& loci = names[qname];

    string fullname;
    if (qname.second.length() == 0) {
      fullname = qname.first;
    } else {
      fullname = "<<" + qname.first + ">>" + qname.second;
    }

    const vector<uint64_t> &counts = loci.first;
    if (display_percent()) {
      bool is_significant = false;
      for (j = 0; j < counts.size(); ++j) {
	if (visible(j)) {
	  if (n_sample(j) &&
	      round_to_nearest_int(
				   decimalfactor * 100.0 * counts[j]/n_sample(j)))
	    is_significant = true;
	}
      }
      if (is_significant) {
	for (j = 0; j < counts.size(); ++j) {
	  if (visible(j)) {
	    if (n_sample(j)) {
	      int value = 
		round_to_nearest_int(decimalfactor * 100.0
				     * counts[j]/n_sample(j));
	      o << ' ' << setw(widthcounts) << value/decimalfactor;
	      if (ndecimal) {
		char oldfill = o.fill('0');
		o << '.' << setw(ndecimal)
		  << value%decimalfactor;
		o.fill(oldfill);
	      }
	      o << '%';
	    }
	    else {
	      o << "   0";
	      if (ndecimal) {
		o << ' ' << setw(ndecimal) << ' ';
	      }
	    }
	  }
	}
	o << ' ' << html_linkline(fullname, loci.second);;
	o << endl;
      }
    }
    else {
      for (j = 0; j < counts.size(); ++j) {
	if (visible(j)) {
	  o << ' ' << setw(widthcounts);
	  o << counts[j];
	}
      }
      o << ' ' << html_linkline(fullname, loci.second);
      o << endl;
    }
  }

  if (html()) o << "</pre>" << endl;
}

void
Summary::add_filesearch(const FileSearch &fs)
{
  filesearches_.push_back(fs);
}

std::string
Summary::simple_resolve(const std::string &file) const
{
  string resolved_file;

  for (uint i = 0; i < filesearches_.size(); ++i) {
    resolved_file = filesearches_[i].resolve(file);
    if (resolved_file.size()) return resolved_file;
  }

  return resolved_file;
}

std::string
Summary::resolve(const std::string &file) const
{
  if (resolve_saved_.find(file) != resolve_saved_.end()) {
    return resolve_saved_[file];
  }

  string r = simple_resolve(file);
  if (r.size() > 0) { resolve_saved_[file] = r; return r; }

  if (file_exists(file)) return file;

  // check if file is a mangled file name produced by KCC
  uint suffix_length = 11+1+3+1+1;
  if (file.size() > suffix_length) {
    if (file.substr(file.size()-6,5) == ".int.") {
      string demangled_file = file.substr(0,file.size()-suffix_length);
      std::string r = simple_resolve(demangled_file);
      if (r.size() > 0) { resolve_saved_[file] = r; return r; }
      if (file_exists(demangled_file)) {
	resolve_saved_[file] = demangled_file;
	return demangled_file;
      }
    }
  }

  // give up and return the original file name
  resolve_saved_[file] = file;
  return file;
}

std::string
Summary::html_filename(const std::string &filename) const
{
  string ret(resolve(filename));
  replace(ret.begin(), ret.end(), '/', '_');
  ret.append(".html");
  return ret;
}

std::string
Summary::html_linkline(const std::string &text,
                       const std::set<location> &loc) const
{
  if (!html() || loc.size() == 0) return text;

  // this tries to reduce the html output into a reasonable
  // size by only listing the references to the lowest
  // number line within each file location

  map<string,uint> foundfiles;
  for (set<location>::const_iterator iloc = loc.begin();
       iloc != loc.end();
       ++iloc) {
    string filename, unix_filename;
    uint lineno;
    unix_filename = (*iloc).first;
    filename = html_filename(unix_filename);
    lineno = (*iloc).second;
    if (file_found(unix_filename)) {
      map<string,uint>::iterator loc = foundfiles.find(filename);
      if (loc != foundfiles.end()) {
	if (lineno > 0 && (*loc).second > lineno) {
	  (*loc).second = lineno;
	}
      }
      else {
	foundfiles[filename] = lineno;
      }
    }
  }

  if (foundfiles.size() == 0) return text;

  if (foundfiles.size() == 1) {
    ostringstream o;
    o << "<a href=\""
      << (*foundfiles.begin()).first;
    if ((*foundfiles.begin()).second > 0) {
      o << "#" << (*foundfiles.begin()).second;
    }
    o << "\">"
      << text
      << "</a>";
    return o.str();
  }

  ostringstream o;

  int i=1;
  o << text << "[";
  for (map<string, uint>::iterator iloc = foundfiles.begin();
       iloc != foundfiles.end();
       ++i) {
    o << "<a href=\""
      << (*iloc).first;
    if ((*iloc).second > 0) o << "#" << (*iloc).second;
    o << "\">"
      << i
      << "</a>";
    iloc++;
    if (iloc != foundfiles.end()) o << " ";
  }
  o << "]";

  return o.str();
}

