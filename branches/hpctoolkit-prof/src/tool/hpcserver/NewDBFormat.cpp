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
// Copyright ((c)) 2002-2015, Rice University
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

// This file adds support for the new binary db format to the trace
// server.  It's really only the index for the start/end trace offsets
// and the proc-id/trace-id labels that change, plus a few odds and
// ends.  The bulk of the trace data is the same.
//
// Todo:
//
// 1. Read threads.db for the process/thread indicies.
//
// 2. Check consistency of offsets.end.  The db files treat end as
// exclusive, but the server seems to use inclusive.
//
// 3. Sometimes the client freezes up or displays an odd depth view,
// but this happens even with an old style database.

//***************************************************************************

#include <sys/types.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <include/big-endian.h>

#include "Constants.hpp"
#include "DBOpener.hpp"
#include "DebugUtils.hpp"
#include "FileData.hpp"
#include "FileUtils.hpp"

#define EXPERIMENT_XML  "experiment.xml"
#define TRACE_DB    "trace.db"
#define THREADS_DB  "threads.db"

//***************************************************************************

// Structs defining the format of the binary db files.
// FIXME: this should be in a common header file.

#define FILE_VERSION  3

#define TRACE_FMT     2
#define THREADS_FMT   4

#define HEADER_SIZE  512
#define COMMON_SIZE  256
#define MESSAGE_SIZE  32

#define MAGIC  0x06870630

struct __attribute__ ((packed)) common_header {
    char      mesg[MESSAGE_SIZE];
    uint64_t  magic;
    uint64_t  version;
    uint64_t  type;
    uint64_t  format;
    uint64_t  num_cctid;
    uint64_t  num_metid;
    uint64_t  num_threads;
  };

struct __attribute__ ((packed)) trace_header {
    uint64_t  index_start;
    uint64_t  index_length;
    uint64_t  trace_start;
    uint64_t  trace_length;
    uint64_t  min_time;
    uint64_t  max_time;
    uint32_t  size_offset;
    uint32_t  size_length;
    uint32_t  size_global_tid;
    uint32_t  size_time;
    uint32_t  size_cctid;
};

struct __attribute__ ((packed)) trace_index {
    uint64_t  offset;
    uint64_t  length;
    uint64_t  global_tid;
};

//***************************************************************************

using namespace std;

namespace TraceviewerServer
{

// Fill in the BaseDataFile class.  Read the index in trace.db and
// compute the offsets[], processIDs[] and threadIDs[] arrays.
//
// Fixme: need to read threads.db to get the real process/thread IDs
// instead of just 0..N-1.
//
void
BaseDataFile::setNewData(FileData *locations, int headerSize)
{
    const char *trace_str = locations->fileTrace.c_str();
    char header[HEADER_SIZE];
    long len;

    //
    // Step 1 -- open trace.db and read the header.
    //
    int trace_fd = open(trace_str, O_RDONLY);
    if (trace_fd < 0) {
        err(1, "unable to open: %s", trace_str);
    }

    len = read(trace_fd, header, HEADER_SIZE);
    if (len != HEADER_SIZE) {
        err(1, "unable to read: %s", trace_str);
    }

    struct common_header *common_hdr = (struct common_header *) header;
    struct trace_header *trace_hdr = (struct trace_header *) (header + COMMON_SIZE);

    uint64_t magic = be_to_host_64(common_hdr->magic);
    uint64_t format = be_to_host_64(common_hdr->format);
    uint64_t num_threads = be_to_host_64(common_hdr->num_threads);

    if (magic != MAGIC) {
        errx(1, "bad magic value (0x%lx) in: %s", magic, trace_str);
    }
    if (format != TRACE_FMT) {
        errx(1, "bad format value (%ld) in: %s", format, trace_str);
    }

    //
    // Step 2 -- read the index and fill in arrays.
    //
    numFiles = num_threads;

    processIDs = new int[num_threads];
    threadIDs = new short[num_threads];
    offsets = new OffsetPair[num_threads];

    if (processIDs == NULL || threadIDs == NULL || offsets == NULL) {
	errx(1, "out of memory in BaseDataFile::setNewData");
    }

    // fixme: need to read process/thread IDs from threads.db.

    uint64_t index_start = be_to_host_64(trace_hdr->index_start);
    uint64_t index_length = num_threads * sizeof(struct trace_index);

    struct trace_index *index = (struct trace_index *) malloc(index_length);
    if (index == NULL) {
	errx(1, "out of memory in BaseDataFile::setNewData");
    }

    len = lseek(trace_fd, index_start, SEEK_SET);
    if (len != (long) index_start) {
        err(1, "unable to seek to trace index (0x%lx) in setNewData",
	    index_start);
    }

    len = read(trace_fd, index, index_length);
    if (len != (long) index_length) {
        err(1, "unable to read trace index in: %s", trace_str);
    }

    // fixme: is value for offsets.end inclusive or exclusive?

    for (ulong k = 0; k < num_threads; k++) {
	offsets[k].start = be_to_host_64(index[k].offset);
	offsets[k].end = offsets[k].start + be_to_host_64(index[k].length) - 12;
	processIDs[k] = k;
	threadIDs[k] = -1;
    }
    type = MULTI_PROCESSES;

    free(index);
    close(trace_fd);

    //
    // Step 3 -- finally, allocate LargeByteBuffer for file.
    //
    masterBuff = new LargeByteBuffer(locations->fileTrace, 1);
}

// Returns: true and fills in 'location' if 'directory' exists and
// contains the new binary db format.
//
bool
DBOpener::verifyNewDatabase(string directory, FileData *location)
{
    // fixme: except for experiment.xml, should read the file names
    // out of the xml file.

    string file_xml = directory + "/" + EXPERIMENT_XML;
    string file_trace = directory + "/" + TRACE_DB;
    string file_threads = directory + "/" + THREADS_DB;
    bool ans = false;

    location->new_db = false;
    location->fileXML = "";
    location->fileTrace = "";
    location->fileThreads = "";

    if (FileUtils::existsAndIsDir(directory)
	&& FileUtils::exists(file_xml)
	&& FileUtils::exists(file_trace)
	&& FileUtils::exists(file_threads))
    {
	location->new_db = true;
	location->fileXML = file_xml;
	location->fileTrace = file_trace;
	location->fileThreads = file_threads;
	ans = true;
    }

    return ans;
}

}  // TraceviewerServer
