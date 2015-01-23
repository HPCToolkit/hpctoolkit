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
#include <lib/prof/DBFormat.h>

#include "Constants.hpp"
#include "DBOpener.hpp"
#include "DebugUtils.hpp"
#include "FileData.hpp"
#include "FileUtils.hpp"

#define EXPERIMENT_XML  "experiment.xml"
#define TRACE_DB    "trace.db"
#define THREADS_DB  "threads.db"

//***************************************************************************

using namespace std;

namespace TraceviewerServer
{

// Read the indicies from trace.db and threads.db and fill in the
// BaseDataFile class: the offsets[], processIDs[] and threadIDs[]
// arrays.
//
void
BaseDataFile::setNewData(FileData *locations, int headerSize)
{
    const char *threads_str = locations->fileThreads.c_str();
    const char *trace_str = locations->fileTrace.c_str();
    char header[HEADER_SIZE];
    long len;

    //
    // Step 1 -- open threads.db and read the threads index table.
    //
    int threads_fd = open(threads_str, O_RDONLY);
    if (threads_fd < 0) {
        err(1, "unable to open: %s", threads_str);
    }

    len = read(threads_fd, header, HEADER_SIZE);
    if (len != HEADER_SIZE) {
        err(1, "unable to read: %s", threads_str);
    }

    common_header_t *common_hdr = (common_header_t *) header;
    threads_header_t *threads_hdr = (threads_header_t *) (header + COMMON_SIZE);

    uint64_t magic = be_to_host_64(common_hdr->magic);
    uint64_t format = be_to_host_64(common_hdr->format);
    uint64_t total_threads = be_to_host_64(common_hdr->num_threads);

    if (magic != MAGIC) {
        errx(1, "bad magic value (0x%lx) in: %s", magic, threads_str);
    }
    if (format != THREADS_FMT) {
        errx(1, "bad format value (%ld) in: %s", format, threads_str);
    }

    uint32_t num_fields = be_to_host_32(threads_hdr->num_fields);
    uint64_t index_start = be_to_host_64(threads_hdr->index_start);
    uint64_t index_size = total_threads * num_fields * sizeof(uint32_t);

    if (num_fields < 1) {
	errx(1, "bad num_fields value (%d) in: %s", num_fields, threads_str);
    }
    if (num_fields > 2) {
	warnx("bad num_fields value (%d) in: %s", num_fields, threads_str);
    }

    uint32_t * threads_index = (uint32_t *) malloc(index_size);
    if (threads_index == NULL) {
	errx(1, "out of memory in BaseDataFile::setNewData");
    }

    len = lseek(threads_fd, index_start, SEEK_SET);
    if (len != (long) index_start) {
        err(1, "unable to seek to threads index (0x%lx) in: %s",
	    index_start, threads_str);
    }

    len = read(threads_fd, threads_index, index_size);
    if (len != (long) index_size) {
        err(1, "unable to read threads index in: %s", threads_str);
    }

    close(threads_fd);

    //
    // Step 2 -- open trace.db and read the header.
    //
    int trace_fd = open(trace_str, O_RDONLY);
    if (trace_fd < 0) {
        err(1, "unable to open: %s", trace_str);
    }

    len = read(trace_fd, header, HEADER_SIZE);
    if (len != HEADER_SIZE) {
        err(1, "unable to read: %s", trace_str);
    }

    trace_header_t * trace_hdr = (trace_header_t *) (header + COMMON_SIZE);

    magic = be_to_host_64(common_hdr->magic);
    format = be_to_host_64(common_hdr->format);
    uint64_t active_threads = be_to_host_64(common_hdr->num_threads);

    if (magic != MAGIC) {
        errx(1, "bad magic value (0x%lx) in: %s", magic, trace_str);
    }
    if (format != TRACE_FMT) {
        errx(1, "bad format value (%ld) in: %s", format, trace_str);
    }

    //
    // Step 3 -- read the trace index and fill in arrays.
    //
    numFiles = active_threads;

    processIDs = new int[active_threads];
    threadIDs = new short[active_threads];
    offsets = new OffsetPair[active_threads];

    if (processIDs == NULL || threadIDs == NULL || offsets == NULL) {
	errx(1, "out of memory in BaseDataFile::setNewData");
    }

    index_start = be_to_host_64(trace_hdr->index_start);
    index_size = active_threads * sizeof(trace_index_t);

    trace_index_t * index = (trace_index_t *) malloc(index_size);
    if (index == NULL) {
	errx(1, "out of memory in BaseDataFile::setNewData");
    }

    len = lseek(trace_fd, index_start, SEEK_SET);
    if (len != (long) index_start) {
        err(1, "unable to seek to trace index (0x%lx) in: %s",
	    index_start, trace_str);
    }

    len = read(trace_fd, index, index_size);
    if (len != (long) index_size) {
        err(1, "unable to read trace index in: %s", trace_str);
    }

    // note: the trace server treats offsets.end as inclusive, it
    // points to the beginning of the last valid trace record.

    for (ulong k = 0; k < active_threads; k++) {
	uint64_t tid = be_to_host_64(index[k].global_tid);

	offsets[k].start = be_to_host_64(index[k].offset);
	offsets[k].end = offsets[k].start + be_to_host_64(index[k].length) - 12;

	processIDs[k] = be_to_host_32(threads_index[tid * num_fields]);
	if (num_fields > 1) {
	    threadIDs[k] = be_to_host_32(threads_index[tid * num_fields + 1]);
	} else {
	    threadIDs[k] = -1;
	}
    }
    type = MULTI_PROCESSES;
    if (num_fields > 1) {
        type |= MULTI_THREADING;
    }

    free(threads_index);
    free(index);
    close(trace_fd);

    //
    // Step 4 -- finally, allocate LargeByteBuffer for file.
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
