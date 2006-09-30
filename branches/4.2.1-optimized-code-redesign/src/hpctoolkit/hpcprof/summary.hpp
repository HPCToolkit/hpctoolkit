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
//    (summary.h).
//
//***************************************************************************

/** @file */
#ifndef _summary_h
#define _summary_h

//************************* System Include Files ****************************

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>

#include <unistd.h>

//*************************** User Include Files ****************************

#include "loadmodule.hpp"

#include <lib/prof-juicy/FlatProfileReader.hpp>

//*************************** Forward Declarations **************************

//***************************************************************************

//////////////////////////////////////////////////////////////////////////
// utility functions

/** Takes a string and modifies it so it will display correctly
    as html.  For example, the "&" character will be converted
    into "&amp;".
 */
std::string htmlize(const std::string &);

//////////////////////////////////////////////////////////////////////////
// Event

/** A profiled event.
*/
class Event {
  protected:
    std::string name_;
    std::string description_;
    uint64_t period_;
  public:
    /// Construct a Event using a papi_event_t structure.
    Event(const char* name, const char* desc, uint64_t period);
    /** Destroy the Event. This does not delete the papi_event_t given
        to the constructor. */
    virtual ~Event();
    /// Returns the event name.
    virtual const char *name() const;
    /// Returns the event description.
    virtual const char *description() const;
    /// Returns the profiling sampling period
    uint64_t period() const;
};

/** This is a specialization of Event that represents aggregate
    events.  An aggregate event is one of a mininum, maximum, or a sum
    over a set of events.  Note that CollectiveEvents only make sense
    for two or more Events with the same period.
*/
class CollectiveEvent: public Event {
  public:
    /// This enum is used to represent the event type.
    enum Op { Min, Max, Sum };
  protected:
    /// The name of the aggregate event.
    std::string name_;
    /// The description of the aggregate event.
    std::string description_;
    /// The type of aggregate event.
    Op op_;
  public:
    /** Construct a Event using a papi_event_t structure and an
        aggregate event type. */
    CollectiveEvent(const Event*, Op);
    /// Destroy the aggregate event.
    ~CollectiveEvent();
    /// Returns the aggregate event name.
    const char *name() const;
    /// Returns the aggregate event description.
    const char *description() const;
};

//////////////////////////////////////////////////////////////////////////
// typedefs for collecting counter data

/** The funcmap type maps the function name to the counts for a line of
    code.  This is a map because there might be more than one function
    attributed to a line of code due to inlining.  */
typedef std::map<std::string, std::vector<uint64_t> > funcmap_t;

/** The linemap type maps the line number to a funcmap. */
typedef std::map<unsigned int, funcmap_t> linemap_t;

/** The filemap type maps file names to linemaps. */
typedef std::map<std::string, linemap_t> filemap_t;

/** The lmmap type maps load module names to filemaps.  It contains
    all of the data available for all counters. */
typedef std::map<std::string, filemap_t> lmmap_t;


//////////////////////////////////////////////////////////////////////////
// namemap typedefs

// Qualified name.  We want to keep important parts of a qualified
// name separate.  Typically the first element is the load module name
// while the second is a file name, function name, or file name with a
// line number (as a string).
typedef std::pair<std::string, std::string> qual_name;

typedef std::map<std::string, std::string>::value_type qual_name_val;

struct lt_qual_name
{
  // return true if x < y; false otherwise
  bool operator()(const qual_name& x, const qual_name& y) const
  {
    if (x.first == y.first) {
      return (x.second < y.second);
    } else {
      return (x.first < y.first);
    }
  }
};


/** The location type pairs a filename with a line number. */
typedef std::pair<std::string, unsigned int> location;

/** The loccount type pairs a set of locations with the a counter value
    vector.  It is used to represent aggregate counts from several
    locations.  */
typedef std::pair<std::vector<uint64_t>, std::set<location> > loccount;

/** The namemap type aggregates all of the counter information for a
    particular qual_name. */
typedef std::map<qual_name, loccount, lt_qual_name> namemap;

typedef std::map<qual_name, loccount, lt_qual_name>::value_type
  namemap_val;


/** The FileSearch class is used to locate source file names. */
class FileSearch {
    /// The directory in which to start the search.
    std::string directory_;
    /// If true, recusively search subdirectories.
    bool recursive_;
    /// Used internally to resolve file names.
    std::string resolve(const std::string &dir, const std::string &file) const;
  public:
    /** Construct a FileSearch object.
        @param directory The name of the directory to search.
        @param recursive If true, search subdirectories.
    */
    FileSearch(const std::string &directory = "",
               bool recursive = false);
    /// Returns the directory that will be searched.
    const std::string &directory() const { return directory_; }
    /// Returns true if subdirectories are to be recursively searched.
    bool recursive() const { return recursive_; }

    /** Searches for a file.
        @param file The name of the file to find.
        @return The name of the found file, or the given file if the file
        was not found.
    */
    std::string resolve(const std::string &file) const;
};

/** The CollectiveLocations class is used to keep track of which counters
    correspond to aggregates of other counters.  
*/
class CollectiveLocations {
  private:
    int lsum_; //< The location of the sum of the aggregated counters.
    int lmax_; //< The location of the maximum of the aggregated counters.
    int lmin_; //< The location of the minimum of the aggregated counters.
    int nevents_; //< The number of events/counters aggregated.
    //const Event* event_; //< The type of event aggregated.
    std::string eventname_; //< The type of event aggregated.
    
    /// The locations/indices of the elementary events.
    std::vector<int> elementary_locations_;

  public:
    CollectiveLocations() {
      nevents_ = lsum_ = lmax_ = lmin_ = 0;
      //event_ = NULL;
    }
    /** Add another elementary event to be aggregated.  @param i The index
        for the new event.  @param e The event type.  @param offset The
        offset for the next available event slot.  If needed, slots will be
        allocated for min, max, and sum events and offset will be
        incremented.  */
    void set_offset(int i, /*const Event* e*/ const char* ename, int &offset) {
      elementary_locations_.push_back(i);
      //event_ = e;
      eventname_ = ename;
      nevents_++;
      if (nevents_ == 2) {
          lmin_ = offset++;
          lmax_ = offset++;
          lsum_ = offset++;
        }
    }
    /** Adjust the min, max, and sum event offsets.
        @param off The offset.
    */
    void inc_offset(int off) {
      lmin_ += off;
      lmax_ += off;
      lsum_ += off;
    }
    /// Returns the slot for the minimum counter.
    int lmin() const { return lmin_; }
    /// Returns the slot for the maximum counter.
    int lmax() const { return lmax_; }
    /// Returns the slot for the sum counter.
    int lsum() const { return lsum_; }
    /// Returns the number of aggregated events.
    int nevents() const { return nevents_; }
    /// Returns the event information for the aggregated event.
    //const Event* event() const { return event_; }
    /** Looks up the offsets for the elementary events that this
        CollectiveLocations aggregates.
        @param i The number of the elementary event.
        @return The slot where the elementary event is stored.  */
    int elementary_location(int i) const { return elementary_locations_[i]; }
};

/** The Summary class stores all known information about the
    counters.  It is used to display the information from
    a variety of perspectives.
*/
class Summary {
   private:
  
    std::string pgm_name_; // program name
  
    // These vectors contain data that corresponds to all of the
    // elementary and collective events for the summary.  The data is
    // correlated by vector index.  Data for elementary events comes
    // first, followed by collective events (if any).  Each event has
    // a corresponding counter, either elementary or collective.
    
    int n_event_;      // number of events (including collective events)
    int n_collective_; // number of collective events

    // (vectors indexed by event/counter number)
    std::vector<Event*> event_;                //< The events.
    std::vector<uint64_t> n_sample_;       //< The number of samples.
    std::vector<uint64_t> outofrange_;     //< The number out of range.
    std::vector<uint64_t> overflow_;       //< The number of overflows.
    std::vector<uint64_t> n_missing_file_; //< The number missing a file.
    std::vector<uint64_t> n_missing_func_; //< The number missing a func.
    std::vector<bool> visible_;                //< True, if to be shown.
    std::vector<bool> elementary_;             //< True, if elementary

    std::vector<FileSearch> filesearches_;     //< Used to find source files.

    /** Maps events names to CollectiveLocations.   ColletiveLocations
        describes where the collective operations for that event type
        are stored. */
    std::map<std::string, CollectiveLocations> event2locs_map_;

    bool html_; //< If true, generate html output.

    // FIXME
    bool line_info_;  //< If true, generate line summaries.
    bool func_info_;  //< If true, generate function summaries.
    bool file_info_;  //< If true, generate file summaries.

    bool display_percent_; //< If true, use percentages, else use total counts.

    int debug_; //< If nonzero, print debuggin info.

    time_t prof_mtime_; //< The oldest prof data file time. 
    time_t exec_mtime_; //< The load module time.   // FIXME  ((time_t)-1)

    lmmap_t lmmap_; //< Stores all of the counter information.

    /// A cache of all resolved filenames. FIXME (duplicates)
    mutable std::map<std::string,std::string> resolve_saved_;

  private:
    /// Cleans up data and restores default settings.
    void cleanup();
    /// Initialization code common to all constructors.
    void ctor_init();

  public:

    /** Construct a Summary object.  @param debug Sets the debugging level. */
    Summary(int debug = 0);
    /** Construct a Summary object.
        @param p The exectable.
        @param v A vector of vmon data files.
        @param debug The debugging level.
     */
    Summary(const LoadModule *e, const std::vector<ProfFile*>& v,
	    int debug = 0);
    /** Construct a Summary object.
        @param p Name of the main program binary
        @param v A vector of vmon data file names.
        @param debug The debugging level.
     */
    Summary(const std::string& e, const std::vector<std::string>& v,
	    int debug = 0);
    ~Summary();

    /** Initialize this Summary object.
        @param p Name of the main program binary
        @param v A vector of prof data file names.
     */
    bool init(const std::string&, const std::vector<std::string>&);

    /** Initialize this Summary object.
        @param p Name of the main program binary
        @param v A vector of ProfFiles.
     */
    bool init(const std::string&, const std::vector<ProfFile*>&);

    bool process_lm(const ProfFileLM&, int);

    /** Set the debugging level. @param d The debugging level. */
    void set_debug(int d) { debug_ = d; }

    /** Used to control whether or not html output is generated.
        @param b If true, html will be generated.
    */
    void set_html(bool b) { html_ = b; }
    /// Returns true if html is to be generated.
    bool html() const { return html_; }
    /** Converts a file name into another file name that will be used to
        store the annotated html version of that file.  Slashes are changed
        to underscores, and a ".html" is appended.  */
    std::string html_filename(const std::string &filename) const;
    /** Generates hyperlinks to a set of locations.  When several
        lines in the same file are to be linked, only the lowest
        numbered line is linked.  If more than one file is to be
        linked, these are linked with a series of digits in brackets
        after the text.  Multiple files can arise due to function
        inlining.
        @param text The text to be displayed in the hyperlink.
        @param locs The set of location data.
     */
    std::string html_linkline(const std::string &text,
                              const std::set<location> &locs) const;

    /// Returns true if line information is to be summarized.
    bool line_info() const { return line_info_; }
    /// Returns true if file information is to be summarized.
    bool file_info() const { return file_info_; }
    /// Returns true if function information is to be summarized.
    bool func_info() const { return func_info_; }

    /// Returns true if total percentages are displayed, rather than counts.
    bool display_percent() const { return display_percent_; }
    /// Controls whether or not percentages are displayed.
    void set_display_percent(bool d) { display_percent_ = d; }

    /** Returns the filemap which contains all information about the
        counters and locations. */
    const lmmap_t &lmmap() const { return lmmap_; }

    void construct_loadmodule_namemap(namemap &nm);
    /** Construct a namemap that maps file names to all the
        counter information about that file.
        @param nm The resulting namemap. */
    void construct_file_namemap(namemap &nm);
    /** Construct a namemap that maps lines (represented as
        "filename:number") to all the counter information about that line.
        @param nm The resulting namemap. */
    void construct_line_namemap(namemap &nm);
    /** Construct a namemap that maps function names to all the
        counter information about that function.
        @param nm The resulting namemap. */
    void construct_func_namemap(namemap &nm);

    /** Write a load module summary. @param o Writes the summary here. */
    void summarize_loadmodules(std::ostream &o = std::cout);
    /** Write a file summary. @param o Writes the summary here. */
    void summarize_files(std::ostream &o = std::cout);
    /** Write a line summary. @param o Writes the summary here. */
    void summarize_lines(std::ostream &o = std::cout);
    /** Write a function summary. @param o Writes the summary here. */
    void summarize_funcs(std::ostream &o = std::cout);
    /** Write a generic summary. @param o Writes the summary here. */
    void summarize_generic(void (Summary::*construct)(namemap&),
                           std::ostream &o=std::cout);

    /** Returns the executable modification time. */
    time_t exec_mtime() const { return exec_mtime_; }
    /** Returns the oldest prof time. */
    time_t prof_oldest_mtime() const { return prof_mtime_; }

    /// Returns the number of counters (including aggregates).
    int ncounter() const { return n_event_; } 
    /// Returns the number of samples for the given counter.
    uint64_t n_sample(int i) const { return n_sample_[i]; }
    /// Returns the number of overflows for the given counter.
    uint64_t overflow(int i) const { return overflow_[i]; }
    /// Returns the number of out of range samples for the given counter.
    uint64_t outofrange(int i) const { return outofrange_[i]; }
    /// Returns the number of missing files for the given counter.
    uint64_t n_missing_file(int i) const { return n_missing_file_[i]; }
    /// Returns the number of missing function names for the given counter.
    uint64_t n_missing_func(int i) const { return n_missing_func_[i]; }

    /// Returns true if the given counter is visible.
    bool visible(int i) const { return visible_[i]; }
    /// Returns the number of visible counters.
    int nvisible() const;

    /// Make the given counter visible.
    void show(int i) { visible_[i] = true; }
    /// Make all the counters visible.
    void show_all();
    /// Show the collective (aggregate) counters.
    void show_collective();
    /// Show all the elementary (nonaggregate) counters.
    void show_collected();
    /** Show only the aggregate counters if the number of
        aggregrated counters is greater than or equal to
        the threshold.
        @param threshold The threshold for showing aggregate events.
     */
    void show_only_collective(int threshold=4);

    /// Hide the given counter.
    void hide(int i) { visible_[i] = false; }
    /// Hide all counters.
    void hide_all();
    /// Hide the collective (aggregate) counters.
    void hide_collective();
    /// Hide the elementary (nonaggregate) counters.
    void hide_collected();

    /// Add another FileSearch object to locate source files.
    void add_filesearch(const FileSearch &);
    /** Locates the given file name using all the FileSearch given
        with the add_filesearch member. */
    std::string simple_resolve(const std::string &file) const;
    /** Locates the given file using simple_resolve.  This does
        further processing to try to locate files which have mangled
        names.  This happens when using KCC. */
    std::string resolve(const std::string &file) const;

    /** Return the event corresponding to the given counter. */
    const Event& event(int i) { return *event_[i]; }

    /** Like resolve, but returns "" if the file does not exist. */
    std::string find_file(const std::string &) const;
    /** Return true if the given file is found. */
    bool file_found(const std::string &) const;
    /** Annotate a file, displaying visible counters for each line.
	Returns false on error; true otherwise.
        @param qname The <load module name, file name> to be annotated.
        @param as The ostream where the annotated file will be written.
     */
    bool annotate_file(const qual_name& qname,
                       std::ostream &as = std::cout) const;
    /** Annotate a file, displaying visible counters for each line.
	Returns false on error; true otherwise.
        @param s An input stream for file to be annotated.
        @param as The ostream where the annotated file will be written.
        @param lines The linemap giving the counters for each line.
     */
    bool annotate_file(std::istream &s, std::ostream &as,
                       const linemap_t &linemap) const;
    /** Print all the summary information.  This is mainly for
        debugging. */
    void print(std::ostream &o = std::cout) const;
};

#endif
