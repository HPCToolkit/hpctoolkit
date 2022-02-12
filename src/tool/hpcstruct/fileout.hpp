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
// Copyright ((c)) 2002-2022, Rice University
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
  void open() {
    if (!stream_name.empty()) {
      stream = IOUtil::OpenOStream(stream_name.c_str());
      buffer = new char[HPCIO_RWBufferSz];
      stream->rdbuf()->pubsetbuf(buffer, HPCIO_RWBufferSz);
    }
  };
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
	if ( ( global_args->cache_stat != CACHE_DISABLED) && ( global_args->cache_stat != CACHE_NOT_NAMED) ) {
          global_args->cache_stat = CACHE_ENTRY_ADDED;
        }
      }
    }
    return needed;
  };
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
