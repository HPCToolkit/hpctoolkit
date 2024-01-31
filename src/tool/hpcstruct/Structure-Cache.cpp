// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2024, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

//***************************************************************************
//
// File:
//   Structure-Cache.cpp
//
// Purpose:
//   functions that support management of a cache for hpcstruct files
//
//***************************************************************************

//***************************************************************************
// global includes
//***************************************************************************

#include <dirent.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <cstring>
#include <memory>

#include <xercesc/sax2/SAX2XMLReader.hpp>
#include <xercesc/sax2/DefaultHandler.hpp>
#include <xercesc/sax2/XMLReaderFactory.hpp>
#include <xercesc/sax2/Attributes.hpp>
#include <xercesc/util/XMLString.hpp>

//***************************************************************************
// local includes
//***************************************************************************

#include "../../lib/prof-lean/elf-hash.h"
#include "../../lib/support/diagnostics.h"
#include "../../lib/support/Exception.hpp"
#include "../../lib/support/FileUtil.hpp"

#include "Structure-Cache.hpp"
#include "Structure-Version.hpp"
#include "Args.hpp"

using namespace std;
using namespace xercesc;

//***************************************************************************
// environment variable naming the structure cache; may be overridden from command line
//***************************************************************************

#define STRUCT_CACHE_ENV "HPCTOOLKIT_HPCSTRUCT_CACHE"



//***************************************************************************
// local operations
//***************************************************************************

// Xerces requires the global XMLPlatformUtils to be called before and after all
// usage. Since that's a big pain, we just tie it into the static constructors.
namespace {
struct XercesState {
  XercesState() {
    XMLPlatformUtils::Initialize();
  }
  ~XercesState() {
    XMLPlatformUtils::Terminate();
  }
};
}

static XercesState xercesState;

typedef enum { PATH_READABLE, PATH_WRITEABLE, PATH_DIR_READABLE, PATH_DIR_WRITEABLE,
        PATH_DIR_CREATED, PATH_ABSENT, PATH_ERROR } ckpath_ret_t;
static  ckpath_ret_t ck_path ( const char *path, const char *caller );
static  ckpath_ret_t mk_dirpath ( const char *path, const char *errortype, bool msg );
static bool check_cache_file (char *path);

// return value
//   1: success
//   0: failure
static int
path_accessible
(
 const char *path,
 int access_flags
)
{
  int status = 0;

  int fd = open(path, access_flags | O_CLOEXEC);
  if (fd != -1) {
    status = 1; // found!
    close(fd); // cleanup
  }

  return status;
}

//  Examine a file-system path and return its state
static ckpath_ret_t
ck_path
(
   const char *path,
   const char *caller
)
{
  ckpath_ret_t ret;

  // See if it's there
  struct stat sb;
  if (stat( path, &sb) == 0 ) {

    // We know it's there
    if ( S_ISDIR(sb.st_mode ) ) {

      // It's a directory; is it writeable?
      if ( access(path, O_RDWR) == 0 ) {
        ret = PATH_DIR_WRITEABLE;
      } else {
        ret = PATH_DIR_READABLE;
      }

    } else {

      // It's a file; is it writeable?
      if ( access(path, O_RDWR) == 0 ) {
        ret = PATH_WRITEABLE;
      } else {
        ret = PATH_READABLE;
      }
    }

  } else {
   // It's just not there

    ret = PATH_ABSENT;
  }
#if 0
  const char *retname;
  switch (ret) {
    case PATH_READABLE:
      retname = "PATH_READABLE = ";
      break;

    case PATH_WRITEABLE:
      retname = "PATH_WRITEABLE = ";
      break;

    case PATH_DIR_READABLE:
      retname = "PATH_DIR_READABLE = ";
      break;

    case PATH_DIR_WRITEABLE:
      retname = "PATH_DIR_WRITEABLE = ";
      break;

    case PATH_DIR_CREATED:
      retname = "PATH_DIR_CREATED = ";
      break;

    case PATH_ABSENT:
      retname = "PATH_ABSENT = ";
      break;

    case PATH_ERROR:
      retname = "PATH_ERROR = ";
      break;

    default:
      retname = "UNKNOWN_RETURN ? = ";
      break;
  }
  std::cerr << "DEBUG " <<caller <<"ck_path ( "<< path << " ) returns " << retname << ret << std::endl;
#endif
  return ret;
 }


//  Make or create a directory path
//  Returns the same state enum as ck_path, above
static ckpath_ret_t
mk_dirpath
(
 const char *path,
 const char *errortype,
 bool msg
)
{
  // First see if it's there
  ckpath_ret_t ret = ck_path(path, "mk_dirpath ");
  if (ret != PATH_ABSENT ) {
    return (ret );
  }

  // path does not exist; create it
  try {
    FileUtil::mkdir(path);
  } catch (Diagnostics::FatalException &e) {
    std::cerr << "ERROR: " << errortype << "\n    " << e.what() << std::endl;
    exit(1);
  }

  return PATH_DIR_CREATED;
}


// determine if there already exists a different hash entry for a file path
//    Returns:
// 1: replacement of existing entry needed
// 0: no cleanup needed

static char *
hpcstruct_cache_needs_cleanup
(
 std::string path,
 const char *hash
)
{
  DIR *dir = opendir(path.c_str());
  char *ret = NULL;

  for (;;) {
    struct dirent *d = readdir(dir);

    if (d == NULL) break;

    // ignore "." and ".."
    if ((strcmp(d->d_name, ".") == 0) ||
        (strcmp(d->d_name, "..") == 0)) continue;

    if (strcmp(hash, d->d_name) == 0) {
      // file is present; make sure it is properly formatted
      char fname[4096];
      sprintf (fname, "%s/%s/hpcstruct", path.c_str(),  d->d_name );
      bool rck = check_cache_file (fname);
      if ( rck == false ) {
        // an improperly formatted file is present
        fprintf (stderr, "  file %s is not a correctly formatted cache file\n", fname );

        // set the name of incorrect file to be returned for replacement
        ret = strdup(d->d_name);
        break;

      } else {
        // fprintf (stderr, "  file %s is a correctly formatted cache file\n", fname );
        continue;       // Shouldn't this be "break;"  ??
      }

    } else {
      ret = strdup(d->d_name);
      break;
    }
  }

  closedir(dir);

  return ret;
}

namespace {
struct XMLStr {
  XMLStr(const std::string& s) : str(XMLString::transcode(s.c_str())) {};
  ~XMLStr() { XMLString::release(&str); }
  operator const XMLCh*() const { return str; }
  XMLCh* str;
};

struct HeaderFinder : public DefaultHandler {
  std::string version;

  void startElement(const XMLCh* const, const XMLCh* const tagname,
                    const XMLCh* const, const Attributes& attrs) {
    char* n = XMLString::transcode(tagname);
    if(n == nullptr) return;

    // We only care about the HPCToolkitStructure tag
    std::string_view name(n);
    if(name != "HPCToolkitStructure") {
      XMLString::release(&n);
      return;
    }
    XMLString::release(&n);

    // Extract the version number. If we fail, the version is "INVALID".
    const XMLCh* const vx = attrs.getValue(XMLStr("version"));
    if(vx == nullptr) {
      version = "INVALID";
      return;
    }
    char* v = XMLString::transcode(vx);
    if(v == nullptr) {
      version = "INVALID";
      return;
    }
    version = v;
    XMLString::release(&v);
  }
};
}

// Routine to check that the cached file is correctly formatted
//  returns true if the cached file seems up-to-date, false otherwise
static bool
check_cache_file (char *path)
{
  // Pass 1: the file must exist, be readable, and end with "</HPCToolkitStructure>"
  try {
    const char expected[] = "</HPCToolkitStructure>\n";
    char got[sizeof expected] = {0};

    std::ifstream file(path, std::ios_base::in | std::ios_base::ate);
    file.seekg(-(std::ifstream::off_type)(sizeof expected - 1), std::ios_base::end);
    file.read(got, sizeof got - 1);
    if(file.gcount() < (std::streamsize)(sizeof expected - 1)) return false;
    if(std::strncmp(expected, got, sizeof expected)) return false;
  } catch(std::exception&) {
    return false;
  }

  // Pass 2: the file must have the a "4.9" version number
  try {
    XMLPScanToken token;
    std::unique_ptr<SAX2XMLReader> parser(XMLReaderFactory::createXMLReader());
    if(!parser) return false;
    if(!parser->parseFirst(XMLStr(path), token)) return false;

    HeaderFinder handler;
    parser->setContentHandler(&handler);
    parser->setErrorHandler(&handler);
    while(handler.version.empty()) {
      if(!parser->parseNext(token)) return false;
    }

    if(handler.version != "4.9") return false;
  } catch(std::exception&) {
    return false;
  } catch(SAXException&) {
    return false;
  }
  // It is properly formatted

  return StructureFileCheckVersion(path);
}

//  clean up cache entry being replaced

static int
hpcstruct_cache_cleanup
(
 const char *cachedir,
 std::string path,
 const char *hash
)
{
  if ( global_args->nocache == true) {
#if 0
    std::cerr << "DEBUG cache_cleanup nocache true, returning 0" << std::endl;
#endif
    return 0;
  }

  char* oldhash = hpcstruct_cache_needs_cleanup(path, hash);

  if (oldhash != NULL) {
    // indicates the name of the replaced entry
    // First, try to remove the directory
    std::string command("rm -rf ");
    command += path;
#if 0
    std::cerr << "DEBUG PATH cleanup running " << command.c_str() << std::endl;
#endif
    system(command.c_str());

    // check to see that it worked
    ckpath_ret_t ret = ck_path ( path.c_str(), "cache_cleanup" );
    if ( ret != PATH_ABSENT ) {
      std::cerr << "ERROR: cache_cleanup of " << path.c_str() << " failed" << std::endl;
      exit(1);
    }

    // indicate that the entry removed from cache
    global_args->cache_stat = CACHE_ENTRY_REMOVED;

    // construct the name for the CACHE/FLAT directory entry
    string fpath =  hpcstruct_cache_flat_entry(cachedir, oldhash );

    // Remove that entry
    command = "rm -f " + fpath;

#if 0
    std::cerr << "DEBUG FLAT cleanup running " << command.c_str() << std::endl;
#endif
    system(command.c_str());

    // check to see that it worked
    ret = ck_path ( fpath.c_str(), "cache_cleanup -- FLAT" );
    if ( ret != PATH_ABSENT ) {
      std::cerr << "ERROR: cache_cleanup FLAT of " << fpath.c_str() << " failed" << std::endl;
      exit(1);
    }
#if 0
    std::cerr << "DEBUG cache_cleanup set state to CACHE_ENTRY_REMOVED = " << CACHE_ENTRY_REMOVED << std::endl;
#endif
  }

  return 0;
}


static bool
empty_string
(
 const char *s
)
{
  return s == 0 || *s == 0;
}



//***************************************************************************
// interface operations
//***************************************************************************

bool
hpcstruct_cache_find
(
 const char *cached_entry
)
{
  bool ret = path_accessible(cached_entry, O_RDONLY);
  if ( ret == false ) {
    return ret;
  }
  return ret;
}


bool
hpcstruct_cache_writable
(
 const char *cache_dir
)
{
  return access(cache_dir, O_RDWR) == 0;
}


char *
hpcstruct_cache_hash
(
 const char *binary_abspath
)
{
  char *eh  = elf_hash(binary_abspath);

  // handle NULL
  if (eh == 0) return strdup("");

  return eh;
}


// Ensure that the cache FLAT subdirectory is created and writeable
//  Returns the path to the entry in the FLAT subdirectory for the input file
//
char *
hpcstruct_cache_flat_entry
(
 const char *cache_dir,
 const char *hash  // hash for elf file
)
{
  std::string path = cache_dir;

  path += "/FLAT";

  ckpath_ret_t ret = mk_dirpath(path.c_str(), "Failed to create hpcstruct cache FLAT directory", true);
  if ( (ret != PATH_DIR_CREATED) && (ret != PATH_DIR_WRITEABLE) ) {
    std::cerr << "ERROR: FLAT directory " << path.c_str() << " access failed ;  returns " << ret << std::endl;
    exit(1);
  }

  // compute the full path to the new cache directory
  path = path + '/' + hash;

  // return the full path for the new cache entry
  return strdup(path.c_str());
}


// Construct the path within the CACHE/PATH for the binary
//
char *
hpcstruct_cache_path_directory
(
 const char *cache_dir,
 const char *binary_abspath,
 const char *hash, // hash for elf file
 const char *suffix
)
{
  std::string path = cache_dir;

  path += "/PATH";

  // early error for path prefix
  ckpath_ret_t ret = mk_dirpath(path.c_str(), "Failed to create hpcstruct cache PATH directory", true);
  if ( (ret != PATH_DIR_CREATED) && (ret != PATH_DIR_WRITEABLE) ) {
    std::cerr << "ERROR: PATH directory " << path.c_str() << " access failed ;  returns " << ret << std::endl;
    exit(1);
  }

  path += binary_abspath;

  // error checking for full path
  mk_dirpath(path.c_str(), "Failed to create entry in hpcstruct cache directory", true);

  // discard any prior entries for path with a different hash
  hpcstruct_cache_cleanup(cache_dir, path.c_str(), hash );

  // compute the full path to the new cache entry's directory
  path = path + '/' + hash;

  // ensure the new cache directory exists
  mk_dirpath(path.c_str(), "Failed to create new hpcstruct cache directory", true);

  // return the full path for the new cache entry's directory
  return strdup(path.c_str());
}


char *
hpcstruct_cache_path_link
(
 const char *binary_abspath,
 const char *hash // hash for elf file
)
{
  std::string path = "../PATH";

  path += binary_abspath;
  path = path + '/' + hash;

  return strdup(path.c_str());
}


char *
hpcstruct_cache_entry
(
 const char *directory,
 const char *kind
)
{
  std::string path = directory;

  // compute the full path for the new cache entry
  path = path + '/' + kind;

  // return the full path for the new cache entry
  return strdup(path.c_str());
}


// Routine to examine arguments, and set up the cache directory
// Returns the absolute path to the top-level cache directory
//  Returns NULL if no cache directory was specified
//
char *
setup_cache_dir
(
 const char *cache_dir,
 Args *args
)
{
  global_args = args;
  static bool warn = true;
  char abspath[PATH_MAX];
  ckpath_ret_t ret;
  bool created = false;

  if (empty_string(cache_dir)) {
    // cache directory is not explicit: consider environment variable value
    cache_dir = getenv(STRUCT_CACHE_ENV);
    if (empty_string(cache_dir)) cache_dir = NULL;
  }
  // invariant: if cache_dir was NULL or empty, it is now NULL

  if ( (cache_dir == NULL) &&  (global_args->nocache == false) ) {
    // no cache directory specified,
    if (warn) {
      DIAG_MsgIf_GENERIC
        ("ADVICE: ", warn, "See the usage message for how to use "
         "a structure cache to accelerate analysis of CPU and GPU binaries\n");
      warn = false;
    }
    return NULL;
  }
  // convert the user path to an absolute path
  for (;;) {
    char* pr = realpath(cache_dir, abspath);
    if (pr != NULL) {
      // the directory now exists, or was created in the previous iteration
      break;
    }
    // The path does not exist
    if (errno == ENOENT) {
      created = true;
      // try to create the partial (or full) path as given in abspath
      ret = mk_dirpath(abspath, "Failed to create hpcstruct cache directory", true);
      if ( ret == PATH_DIR_CREATED ) {
        // the path was created. now see if it's the full path to the cache directory
        continue;
      }
    } else {
      // attempt to create path failed for some other reason
      std::cerr << "ERROR: Failed to create hpcstruct cache directory: " << strerror(errno) << std::endl;
      exit(1);
    }
  }

  if ( created == true ) {
      std::cout << "INFO: Created structure cache directory " << abspath << std::endl << std::endl;
  } else {
     std::cout << "INFO: Using structure cache directory " << abspath << std::endl << std::endl;
  }

  return strdup(abspath);
}
