// -*- C++ -*-

#ifndef _exec_h
#define _exec_h

#ifdef __GNUC__
#pragma interface
#endif

#include <stdio.h>
#include <time.h>
#include <string>
#include <map>

#include "papiprof.h"

// FIXME: Executable -> LoadModule
// FIXME: type for non-BFD exe

/** This represents an executable.  It can be used to lookup
    the file name, line number, and function name corresponding
    to a program counter. */
class Executable {
  public: 
    enum Type {Exe, DSO, Unknown};

  protected:
    /// The debug level.
    int debug_;
    /// FIXME: type
    Type type_; 

  public:
    Executable();
    virtual ~Executable();

    /// Set the debug level.
    void set_debug(int d) { debug_ = d; }
    /// Return the debug level.
    int debug() const { return debug_; }

    /// Return the type.
    Type type() const { return type_; }

    /// Return true if the line information should be accurate.
    virtual bool line_info() const;
    /// Return true if the file information should be accurate.
    virtual bool file_info() const;
    /// Return true if the function information should be accurate.
    virtual bool func_info() const;

    /// Return the modification time of the executable.
    virtual time_t mtime() const = 0;
    /// Return the name of the executable.
    virtual std::string name() const = 0;
    /// Read the given executable.
    virtual void read(const std::string &exec) = 0;
    /// Return the given symbol demangled.
    virtual std::string demangle(const std::string &symbol) const = 0;
    /// Return the offset to the start of the text segment.
    virtual vmon_off_t textstart() const = 0;
    /** Retrieve symbol information about a program counter.
        @param pc The program counter.
        @param filename A pointer to the file name or 0 is written here.
        @param lineno The line number or 0 is written here.
        @param funcname The function name or 0 is written here.
    */
    virtual int find(vmon_off_t pc, const char **filename,
                     unsigned int *lineno, const char **funcname) const = 0;
};

/** This class is used to store information about a function. */
class FuncInfo {
  private:
    std::string name_;
    std::string file_;
    int index_;
  public:
    FuncInfo():index_(0) {};

    void set_name(const std::string &s) { name_ = s; }
    const std::string &name() const { return name_; }
    const char *c_name() const { return name_.c_str(); }

    void set_file(const std::string &s) { file_ = s; }
    const std::string &file() const { return file_; }
    const char *c_file() const { return file_.c_str(); }

    void set_index(int i) { index_ = i; }
    int index() const { return index_; }
};

#define TRUE_FALSE_ALREADY_DEFINED
#include <bfd.h>
/** An executable that uses the GNU BFD to lookup symbolic information. */
class BFDExecutable: public Executable {
  private:
    std::string name_;
    static int bfdinitialized_;

    bfd *bfd_;
    asymbol **symbols_;

    static bool minisyms_;
    static int dynoffset_;

    // used for find address in section
    static bfd_vma pc_;
    static const char *filename_;
    static const char *functionname_;
    static unsigned int line_;
    static bool found_;

    // Using locmap will speed up the case where multiple vmon.out files
    // are used and the BFD bug which requires BFD be reinitialized after
    // most line number lookups exists.
    mutable std::map<vmon_off_t,FuncInfo> locmap_;

    void cleanup(bool clear_cache = true);
    void read_file(const std::string& name, bool clear_cache = true);

    void find_bfd(vmon_off_t pc, const char **filename,
                  unsigned int *lineno, const char **funcname) const;
  public:
    BFDExecutable(const std::string &);
    ~BFDExecutable();

    static void find_address_in_section(bfd*, asection*, PTR);

    time_t mtime() const;
    std::string name() const;
    void read(const std::string &);
    std::string demangle(const std::string &) const;
    vmon_off_t textstart() const;
    int find(vmon_off_t pc, const char **filename,
             unsigned int *lineno, const char **funcname) const;
};

#if 0 /* FIXME: throw this stuff away */
//#ifdef HAVE_LIBLD
#include <ldfcn.h>
#include <aouthdr.h>
#include <syms.h>
/** An executable that uses libld to lookup symbolic information
    for COFF executables. */
class COFFExecutable: public Executable {
  private:
    std::string name_;
    LDFILE *ldfile_;
    vmon_off_t textstart_;
    bool is64_;
    std::map<uint64_t, FuncInfo> locmap_;

    void init32();
    void init64();
    int find32(vmon_off_t pc, const char **filename,
               unsigned int *lineno, const char **funcname) const;
    int find64(vmon_off_t pc, const char **filename,
               unsigned int *lineno, const char **funcname) const;
    void cleanup();
  public:
    COFFExecutable(const std::string &);
    ~COFFExecutable();

    bool is64() const { return is64_; }

    bool line_info() const;

    time_t mtime() const;
    std::string name() const;
    void read(const std::string &);
    std::string demangle(const std::string &) const;
    vmon_off_t textstart() const;
    int find(vmon_off_t pc, const char **filename,
             unsigned int *lineno, const char **funcname) const;
};
//#endif
#endif

#endif
