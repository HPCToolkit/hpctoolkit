// -*-Mode: C++;-*-

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
// Copyright ((c)) 2002-2023, Rice University
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
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

class FileOutputStream {
public:
  FileOutputStream() : stream(0), buffer(0), use_cache(false),
		       is_cached(false) {
  };

  // init is called to set up the output for writing the structure file
  //    The first parameter is the directory under CACHE/PATH into which the cache'd structure file
  //    will be written.  It will be written with the name "kind" in that directory.  For normal
  //    structure files "kind" is "hpcstruct".
  //    For structure files for gpu binaries, analyzed with gpucfg yes, "kind" is "hpcstruct+gpucfg"
  //    For gap files, the name is "gap"
  //
  //    The third parameter is the name under which the symlink to the structure file will be
  //    created in the CACHE/FLAT directory.
  //
  //    The fourth parameter, result, is the name of the output structure file
  //
  //    When the cache is used, the output stream points to the cache'd structure file
  //    When it is not used, the output stream points to the actual output file.
  //
  void init(const char *cache_path_directory, const char *cache_flat_directory, const char *kind,
	    const char *result) {
    name = strdup(result);
    if (cache_path_directory && cache_path_directory[0] != 0) {
      use_cache = true;
      stream_name = hpcstruct_cache_entry(cache_path_directory, kind);
      flat_name = hpcstruct_cache_entry(cache_flat_directory, kind);
    } else {
      stream_name = name;
    }
  };

  // open is called to actually open the output stream for writing.
  //
  void open() {
    if (!stream_name.empty()) {
      stream = IOUtil::OpenOStream(stream_name.c_str());
      buffer = new char[HPCIO_RWBufferSz];
      stream->rdbuf()->pubsetbuf(buffer, HPCIO_RWBufferSz);
    }
  };

  // needed determines whether or not the needed structure file is cache'd.
  bool needed() {
    bool needed = false;
    if (!name.empty()) {
      if (use_cache && (hpcstruct_cache_find(flat_name.c_str()) ||
			hpcstruct_cache_find(stream_name.c_str()))) {
	is_cached = true;
	if ( ( global_args->cache_stat != CACHE_DISABLED) && ( global_args->cache_stat != CACHE_NOT_NAMED) ) {
          global_args->cache_stat = CACHE_ENTRY_COPIED;
        }
      } else {
	needed = true;
#if 0
        std::cerr << "DEBUG needed set true, found cachestat = " << global_args->cache_stat << std::endl;
#endif
	if ( ( global_args->cache_stat != CACHE_DISABLED) && ( global_args->cache_stat != CACHE_NOT_NAMED) ) {
          if ( global_args->cache_stat == CACHE_ENTRY_REMOVED ) {
            // A previous entry at that path was removed
            global_args->cache_stat = CACHE_ENTRY_REPLACED;

          } else {
            // No previous entry existed
            global_args->cache_stat = CACHE_ENTRY_ADDED;
          }
        }
#if 0
        std::cerr << "DEBUG needed set cachestat = " << global_args->cache_stat << std::endl;
#endif
      }
    }
    return needed;
  };

  // Returns the path to the cached file in use, or "" if no cache is available.
  // Calling this disables the automatic copy during finalize(), so only call
  // this if the file needs to be adjusted before use.
  std::string cached() {
    if(use_cache) {
      use_cache = false;
      if(hpcstruct_cache_find(flat_name.c_str())) {
	      return flat_name;
      }
      return stream_name;
    }
    return {};
  }

  // finalize closes the output stream
  void finalize(int error) {
    if (stream) IOUtil::CloseStream(stream);
    if (buffer) delete[] buffer;
    if (!name.empty()) {
      if (error) {
	unlink(name.c_str());
      } else {
	if (use_cache) {
	  if (hpcstruct_cache_find(flat_name.c_str())) {
	    FileUtil::copy(name, flat_name);
	  } else {
	    FileUtil::copy(name, stream_name);
	  }
	}
      }
    }
  };

  std::ostream *getStream() { return stream; };
  std::string &getName() { return name; };

private:
  std::ostream *stream;
  std::string name;
  std::string stream_name;
  std::string flat_name;
  char *buffer;
  bool use_cache;
  bool is_cached;
};
