// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
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

//
// Linux perf mmaped-buffer reading interface
//

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>

/******************************************************************************
 * linux specific includes
 *****************************************************************************/

#include <linux/perf_event.h>
#include <linux/version.h>


/******************************************************************************
 * hpcrun includes
 *****************************************************************************/

#include <hpcrun/messages/messages.h>

/******************************************************************************
 * local include
 *****************************************************************************/

#include "perf_mmap.h"
#include "perf-util.h"
#include "perf_barrier.h"

/******************************************************************************
 * Constants
 *****************************************************************************/

#define MMAP_OFFSET_0            0

#define PERF_DATA_PAGE_EXP        1      // use 2^PERF_DATA_PAGE_EXP pages
#define PERF_DATA_PAGES           (1 << PERF_DATA_PAGE_EXP)

#define PERF_MMAP_SIZE(pagesz)    ((pagesz) * (PERF_DATA_PAGES + 1))
#define PERF_TAIL_MASK(pagesz)    (((pagesz) * PERF_DATA_PAGES) - 1)

#define BUFFER_FRONT(current_perf_mmap)              ((char *) current_perf_mmap + pagesize)
#define BUFFER_SIZE               (tail_mask + 1)
#define BUFFER_OFFSET(tail)       ((tail) & tail_mask)



/******************************************************************************
 * forward declarations
 *****************************************************************************/



/******************************************************************************
 * local variables
 *****************************************************************************/

static int pagesize      = 0;
static size_t tail_mask  = 0;


/******************************************************************************
 * local methods
 *****************************************************************************/




//----------------------------------------------------------
// read from perf_events mmap'ed buffer
// to avoid too many memory fences, the routine requires the current
//  head and tail position. Based on this info, it estimates whether
//  the reading has enough bytes to read or not.
//
// If there is no enough bytes, it returns -1
// otherwise it returns 0
//----------------------------------------------------------
static int
perf_read(u64 data_head, u64 *data_tail,
          pe_mmap_t *current_perf_mmap,
          void *buf,
          size_t bytes_wanted
)
{
  if (current_perf_mmap == NULL)
    return -1;

  u64 tail_position = *data_tail;
  size_t bytes_available = data_head - tail_position;

  if (bytes_wanted > bytes_available) return -1;

  // compute offset of tail in the circular buffer
  unsigned long tail = BUFFER_OFFSET(tail_position);

  long bytes_at_right = BUFFER_SIZE - tail;

  // bytes to copy to the right of tail
  size_t right = bytes_at_right < bytes_wanted ? bytes_at_right : bytes_wanted;

  // front of the circular data buffer
  char *data = BUFFER_FRONT(current_perf_mmap);

  // copy bytes from tail position
  memcpy(buf, data + tail, right);

  // if necessary, wrap and continue copy from left edge of buffer
  if (bytes_wanted > right) {
    size_t left = bytes_wanted - right;
    memcpy(buf + right, data, left);
  }
  *data_tail = tail_position + bytes_wanted;

  return 0;
}


static inline int
perf_read_header(u64 data_head, u64 *data_tail,
  pe_mmap_t *current_perf_mmap,
  pe_header_t *hdr
)
{
  return perf_read(data_head, data_tail, current_perf_mmap, hdr, sizeof(pe_header_t));
}


static inline int
perf_read_u32(u64 data_head, u64 *data_tail,
  pe_mmap_t *current_perf_mmap,
  u32 *val
)
{
  return perf_read(data_head, data_tail, current_perf_mmap, val, sizeof(u32));
}


static inline int
perf_read_u64(u64 data_head, u64 *data_tail,
  pe_mmap_t *current_perf_mmap,
  u64 *val
)
{
  return perf_read(data_head, data_tail, current_perf_mmap, val, sizeof(u64));
}


//----------------------------------------------------------
// special mmap buffer reading for PERF_SAMPLE_READ
//----------------------------------------------------------
static void
handle_struct_read_format(u64 data_head, u64 *data_tail, pe_mmap_t *perf_mmap, int read_format)
{
  u64 value, id, nr, time_enabled, time_running;

  if (read_format & PERF_FORMAT_GROUP) {
    perf_read_u64(data_head, data_tail, perf_mmap, &nr);
  } else {
    perf_read_u64(data_head, data_tail, perf_mmap, &value);
  }

  if (read_format & PERF_FORMAT_TOTAL_TIME_ENABLED) {
    perf_read_u64(data_head, data_tail, perf_mmap, &time_enabled);
  }
  if (read_format & PERF_FORMAT_TOTAL_TIME_RUNNING) {
    perf_read_u64(data_head, data_tail, perf_mmap, &time_running);
  }

  if (read_format & PERF_FORMAT_GROUP) {
    int i;
    for(i=0;i<nr;i++) {
      perf_read_u64(data_head, data_tail, perf_mmap, &value);

      if (read_format & PERF_FORMAT_ID) {
        perf_read_u64(data_head, data_tail, perf_mmap, &id);
      }
    }
  }
  else {
    if (read_format & PERF_FORMAT_ID) {
      perf_read_u64(data_head, data_tail, perf_mmap, &id);
    }
  }
}



//----------------------------------------------------------
// processing of kernel callchains
//----------------------------------------------------------

static int
perf_sample_callchain(u64 data_head, u64 *data_tail,
                      pe_mmap_t *current_perf_mmap, perf_mmap_data_t* mmap_data)
{
  mmap_data->nr = 0;     // initialze the number of records to be 0
  u64 num_records = 0;

  // determine how many frames in the call chain
  if (perf_read_u64(data_head, data_tail, current_perf_mmap, &num_records) == 0) {
    if (num_records > 0) {

      // warning: if the number of frames is bigger than the storage (MAX_CALLCHAIN_FRAMES)
      // we have to truncate them. This is not a good practice, but so far it's the only
      // simplest solution I can come up.
      mmap_data->nr = (num_records < MAX_CALLCHAIN_FRAMES ? num_records : MAX_CALLCHAIN_FRAMES);

      // read the IPs for the frames
      if (perf_read(data_head, data_tail,
                    current_perf_mmap, mmap_data->ips, num_records * sizeof(u64)) != 0) {
        // the data seems invalid
        mmap_data->nr = 0;
        TMSG(LINUX_PERF, "unable to read all %d frames", num_records);
      }
    }
  } else {
    TMSG(LINUX_PERF, "unable to read the number of frames" );
  }
  return mmap_data->nr;
}


/**
 * parse mmapped buffer and copy the values into perf_mmap_data_t mmap_info.
 * we assume mmap_info is already initialized.
 * returns the number of read event attributes
 */
static int
parse_record_buffer(u64 data_head, u64 *data_tail,
                    pe_mmap_t *current_perf_mmap,
                    struct perf_event_attr *attr,
                    perf_mmap_data_t *mmap_info )
{
	int data_read = 0;
	int sample_type = attr->sample_type;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,12,0)
	if (sample_type & PERF_SAMPLE_IDENTIFIER) {
	  perf_read_u64(data_head, data_tail, current_perf_mmap, &mmap_info->sample_id);
	  data_read++;
	}
#endif
	if (sample_type & PERF_SAMPLE_IP) {
	  // to be used by datacentric event
	  perf_read_u64(data_head, data_tail, current_perf_mmap, &mmap_info->ip);
	  data_read++;
	}
	if (sample_type & PERF_SAMPLE_TID) {
	  perf_read_u32(data_head, data_tail, current_perf_mmap, &mmap_info->pid);
	  perf_read_u32(data_head, data_tail, current_perf_mmap, &mmap_info->tid);
	  data_read++;
	}
	if (sample_type & PERF_SAMPLE_TIME) {
	  perf_read_u64(data_head, data_tail, current_perf_mmap, &mmap_info->time);
	  data_read++;
	}
	if (sample_type & PERF_SAMPLE_ADDR) {
	  // to be used by datacentric event
	  perf_read_u64(data_head, data_tail, current_perf_mmap, &mmap_info->addr);
	  data_read++;
	}
	if (sample_type & PERF_SAMPLE_ID) {
	  perf_read_u64(data_head, data_tail, current_perf_mmap, &mmap_info->id);
	  data_read++;
	}
	if (sample_type & PERF_SAMPLE_STREAM_ID) {
	  perf_read_u64(data_head, data_tail, current_perf_mmap, &mmap_info->stream_id);
	  data_read++;
	}
	if (sample_type & PERF_SAMPLE_CPU) {
	  perf_read_u32(data_head, data_tail, current_perf_mmap, &mmap_info->cpu);
	  perf_read_u32(data_head, data_tail, current_perf_mmap, &mmap_info->res);
	  data_read++;
	}
	if (sample_type & PERF_SAMPLE_PERIOD) {
	  perf_read_u64(data_head, data_tail, current_perf_mmap, &mmap_info->period);
	  data_read++;
	}
	if (sample_type & PERF_SAMPLE_READ) {
	  // to be used by datacentric event
	  handle_struct_read_format(data_head, data_tail, current_perf_mmap,
	      attr->read_format);
	  data_read++;
	}
	if (sample_type & PERF_SAMPLE_CALLCHAIN) {
	  // add call chain from the kernel
	  perf_sample_callchain(data_head, data_tail, current_perf_mmap, mmap_info);
	  data_read++;
	}
	if (sample_type & PERF_SAMPLE_RAW) {
	  // not supported at the moment
	  perf_read_u32(data_head, data_tail, current_perf_mmap, &mmap_info->size);
	  mmap_info->data = alloca( sizeof(char) * mmap_info->size );
	  perf_read( data_head, data_tail, current_perf_mmap, mmap_info->data, mmap_info->size) ;
	  data_read++;
	}
	if (sample_type & PERF_SAMPLE_BRANCH_STACK) {
	  data_read++;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
	if (sample_type & PERF_SAMPLE_REGS_USER) {
	  data_read++;
	}
	if (sample_type & PERF_SAMPLE_STACK_USER) {
	  data_read++;
	}
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	if (sample_type & PERF_SAMPLE_WEIGHT) {
	  perf_read_u64(data_head, data_tail, current_perf_mmap, &mmap_info->weight);
	  data_read++;
	}
	if (sample_type & PERF_SAMPLE_DATA_SRC) {
	  perf_read_u64(data_head, data_tail, current_perf_mmap, &mmap_info->data_src);
	  data_read++;
	}
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,0)
	// only available since kernel 3.19
	if (sample_type & PERF_SAMPLE_TRANSACTION) {
	  data_read++;
	}
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0)
	// only available since kernel 3.19
	if (sample_type & PERF_SAMPLE_REGS_INTR) {
	  data_read++;
	}
#endif
	return data_read;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,3,0)
// special parser for smaple_id
// any event that used sample_id_all or perf_record_misc_switch
// needs to be parsed here
static int
parse_sample_id_buffer(u64 data_head, u64 *data_tail,
                        pe_mmap_t *current_perf_mmap,
                        struct perf_event_attr *attr,
                        perf_mmap_data_t *mmap_info )
{
  int data_read = 0;
  int sample_type = attr->sample_type;

  if (sample_type & PERF_SAMPLE_TID) {
    perf_read_u32(data_head, data_tail, current_perf_mmap, &mmap_info->pid);
    perf_read_u32(data_head, data_tail, current_perf_mmap, &mmap_info->tid);
    data_read++;
  }
  if (sample_type & PERF_SAMPLE_TIME) {
    perf_read_u64(data_head, data_tail, current_perf_mmap, &mmap_info->time);
    data_read++;
  }
  if (sample_type & PERF_SAMPLE_TID) {
    perf_read_u32(data_head, data_tail, current_perf_mmap, &mmap_info->pid);
    perf_read_u32(data_head, data_tail, current_perf_mmap, &mmap_info->tid);
    data_read++;
  }
  if (sample_type & PERF_SAMPLE_STREAM_ID) {
    perf_read_u64(data_head, data_tail, current_perf_mmap, &mmap_info->stream_id);
    data_read++;
  }
  if (sample_type & PERF_SAMPLE_CPU) {
    perf_read_u32(data_head, data_tail, current_perf_mmap, &mmap_info->cpu);
    perf_read_u32(data_head, data_tail, current_perf_mmap, &mmap_info->res);
    data_read++;
  }
  if (sample_type & PERF_SAMPLE_IDENTIFIER) {
    perf_read_u64(data_head, data_tail, current_perf_mmap, &mmap_info->sample_id);
    data_read++;
  }

  return data_read;
}
#endif


//----------------------------------------------------------------------
// Public Interfaces
//----------------------------------------------------------------------


//----------------------------------------------------------
// reading mmap buffer from the kernel
// in/out: mmapped data of type perf_mmap_data_t.
// return true if there are more data to be read,
//        false otherwise
//----------------------------------------------------------
int
read_perf_buffer(pe_mmap_t *current_perf_mmap,
                 struct perf_event_attr *attr,
                 perf_mmap_data_t *mmap_info)
{
  pe_header_t hdr;
  u64 data_tail = current_perf_mmap->data_tail;
  u64 data_head = current_perf_mmap->data_head;

  rmb();  // memory fence after reading the data head

  int read_successfully = perf_read_header(data_head, &data_tail, current_perf_mmap, &hdr);
  if (read_successfully != 0 || hdr.size <= 0) {
    return 0;
  }

  mmap_info->header_type = hdr.type;
  mmap_info->header_misc = hdr.misc;

  if (hdr.type == PERF_RECORD_SAMPLE) {
      parse_record_buffer(data_head, &data_tail, current_perf_mmap, attr, mmap_info);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,3,0)
  } else if (hdr.type == PERF_RECORD_SWITCH) {
      // only available since kernel 4.3

    parse_sample_id_buffer(data_head, &data_tail, current_perf_mmap, attr, mmap_info);

    TMSG(LINUX_PERF, "%d context switch %d, time: %u", attr->config, hdr.misc, mmap_info->time);

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4,2,0)
  } else if (hdr.type == PERF_RECORD_LOST_SAMPLES) {
     u64 lost_samples;
     perf_read_u64(current_perf_mmap, &lost_samples);
     TMSG(LINUX_PERF, "[%d] lost samples %d",
    		 current->fd, lost_samples);
     skip_perf_data(current_perf_mmap, hdr.size-sizeof(lost_samples))
#endif

  } else {
      // not a PERF_RECORD_SAMPLE nor PERF_RECORD_SWITCH
      // skip it
      TMSG(LINUX_PERF, "[%d] skip header %d  %d : %d bytes",
    		  attr->config,
    		  hdr.type, hdr.misc, hdr.size);
  }

  // update tail after consuming a record
  rmb();  // memory fence before writing data_tail
  current_perf_mmap->data_tail += hdr.size;

  return (data_tail-current_perf_mmap->data_tail);
}

//----------------------------------------------------------
// allocate mmap for a given file descriptor
//----------------------------------------------------------
pe_mmap_t*
set_mmap(int perf_fd)
{
  if (pagesize == 0) {
    perf_mmap_init();
  }
  void *map_result =
    mmap(NULL, PERF_MMAP_SIZE(pagesize), PROT_WRITE | PROT_READ,
     MAP_SHARED, perf_fd, MMAP_OFFSET_0);

  if (map_result == MAP_FAILED) {
    EMSG("Linux perf mmap failed: %s", strerror(errno));
    return NULL;
  }

  pe_mmap_t *mmap  = (pe_mmap_t *) map_result;

  memset(mmap, 0, sizeof(pe_mmap_t));
  mmap->version = 0;
  mmap->compat_version = 0;
  mmap->data_head = 0;
  mmap->data_tail = 0;

  return mmap;
}

/***
 * unmap buffer. need to call this at the end of the execution
 */
void
perf_unmmap(pe_mmap_t *mmap)
{
  munmap(mmap, PERF_MMAP_SIZE(pagesize));
}

/**
 * initialize perf_mmap.
 * caller needs to call this in the beginning before calling any API.
 */
void
perf_mmap_init()
{
  pagesize = sysconf(_SC_PAGESIZE);
  tail_mask = PERF_TAIL_MASK(pagesize);
}

