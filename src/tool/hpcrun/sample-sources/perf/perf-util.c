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
// Copyright ((c)) 2002-2018, Rice University
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


/******************************************************************************
 * includes
 *****************************************************************************/

#include <linux/version.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/cct_insert_backtrace.h>
#include <lib/support-lean/OSUtil.h>     // hostid

#include <include/linux_info.h>
#include "perf-util.h"

// -----------------------------------------------------
// precise ip / skid options
// -----------------------------------------------------

// Possible value of precise ip:
//  0  SAMPLE_IP can have arbitrary skid.
//  1  SAMPLE_IP must have constant skid.
//  2  SAMPLE_IP requested to have 0 skid.
//  3  SAMPLE_IP must have 0 skid.
//  4  Detect automatically to have the most precise possible (default)
#define HPCRUN_OPTION_PRECISE_IP "HPCRUN_PRECISE_IP"

// default option for precise_ip: autodetect skid
#define PERF_EVENT_AUTODETECT_SKID    4

// constants of precise_ip (see the man page)
#define PERF_EVENT_SKID_ZERO_REQUIRED    3
#define PERF_EVENT_SKID_ZERO_REQUESTED   2
#define PERF_EVENT_SKID_CONSTANT         1
#define PERF_EVENT_SKID_ARBITRARY        0

#define MAX_BUFFER_LINUX_KERNEL 128


//******************************************************************************
// constants
//******************************************************************************

// ordered in increasing precision
const int perf_skid_precision[] = {
  PERF_EVENT_SKID_ARBITRARY,
  PERF_EVENT_SKID_CONSTANT,
  PERF_EVENT_SKID_ZERO_REQUESTED,
  PERF_EVENT_SKID_ZERO_REQUIRED
};

const u64 anomalous_ip = 0xffffffffffffff80;

const int perf_skid_flavors = sizeof(perf_skid_precision)/sizeof(int);


//******************************************************************************
// typedef, structure or enum
//******************************************************************************


//******************************************************************************
// local variables
//******************************************************************************

static uint16_t perf_kernel_lm_id = 0;

static spinlock_t perf_lock = SPINLOCK_UNLOCKED;

static enum perf_ksym_e ksym_status = PERF_UNDEFINED;


//******************************************************************************
// forward declaration
//******************************************************************************



//******************************************************************************
// implementation
//******************************************************************************


/***
 * if the input is a retained (leaf) cct node, return a sibling
 * non-retained proxy.
 */
static cct_node_t *
perf_split_retained_node(
  cct_node_t *node
)
{
  if (!hpcrun_cct_retained(node)) return node;

  // node is retained. the caller of this routine would make it an
  // interior node in the cct, which would cause trouble for hpcprof
  // and hpctraceviewer. instead, use a sibling with that represents
  // the machine code offset +1.

  // extract the abstract address in the node
  cct_addr_t *addr = hpcrun_cct_addr(node);

  // create an abstract address representing the next machine code address 
  cct_addr_t sibling_addr = *addr;
  sibling_addr.ip_norm.lm_ip++;

  // get the necessary sibling to node
  cct_node_t *sibling = hpcrun_cct_insert_addr(hpcrun_cct_parent(node), 
					       &sibling_addr);

  return sibling;
}


/***
 * insert a cct node for a PC in a kernel call path
 */
static cct_node_t *
perf_insert_cct(
  uint16_t lm_id, 
  cct_node_t *parent, 
  u64 ip
)
{
  parent = perf_split_retained_node(parent);

  ip_normalized_t npc;
  memset(&npc, 0, sizeof(ip_normalized_t));
  npc.lm_id = lm_id;
  npc.lm_ip = ip;

  cct_addr_t frm;
  memset(&frm, 0, sizeof(cct_addr_t));
  frm.ip_norm = npc;

  return hpcrun_cct_insert_addr(parent, &frm);
}


/***
 * retrieve the value of kptr_restrict
 */
static int
perf_util_get_kptr_restrict()
{
  static int privilege = -1;

  if (privilege >= 0)
    return privilege;

  FILE *fp = fopen(LINUX_KERNEL_KPTR_RESTICT, "r");
  if (fp != NULL) {
    fscanf(fp, "%d", &privilege);
    fclose(fp);
  }
  return privilege;
}

static uint16_t 
perf_get_kernel_lm_id() 
{
  if (ksym_status == PERF_AVAILABLE && perf_kernel_lm_id == 0) {
    // ensure that this is initialized only once per process
    spinlock_lock(&perf_lock);
    if (perf_kernel_lm_id == 0) {

      // in case of kptr_restrict = 0, we want to copy kernel symbol for each node
      // if the value of kptr_restric != 0, all nodes has <vmlinux> module, and
      //   all calls to the kernel will be from address zero

      if (perf_util_get_kptr_restrict() == 0) {

        char buffer[MAX_BUFFER_LINUX_KERNEL];
        OSUtil_setCustomKernelNameWrap(buffer, MAX_BUFFER_LINUX_KERNEL);
        perf_kernel_lm_id = hpcrun_loadModule_add(buffer);

      } else {
        perf_kernel_lm_id = hpcrun_loadModule_add(LINUX_KERNEL_NAME);

      }
    }
    spinlock_unlock(&perf_lock);
  }
  return perf_kernel_lm_id;
}



//----------------------------------------------------------
// extend a user-mode callchain with kernel frames (if any)
//----------------------------------------------------------
static cct_node_t *
perf_add_kernel_callchain(
  cct_node_t *leaf, 
  void *data_aux
)
{
  cct_node_t *parent = leaf;
  
  if (data_aux == NULL)  {
    return parent;
  }

  perf_mmap_data_t *data = (perf_mmap_data_t*) data_aux;
  if (data->nr > 0) {
    uint16_t kernel_lm_id = perf_get_kernel_lm_id();

    // bug #44 https://github.com/HPCToolkit/hpctoolkit/issues/44 
    // if no kernel symbols are available, collapse the kernel call
    // chain into a single node
    if (perf_util_get_kptr_restrict() != 0) {
      return perf_insert_cct(kernel_lm_id, parent, 0);
    }

    // add kernel IPs to the call chain top down, which is the 
    // reverse of the order in which they appear in ips[]
    for (int i = data->nr - 1; i > 0; i--) {
      parent = perf_insert_cct(kernel_lm_id, parent, data->ips[i]);
    }

    // check ip[0] before adding as it often seems seems to be anomalous
    if (data->ips[0] != anomalous_ip) {
      parent = perf_insert_cct(kernel_lm_id, parent, data->ips[0]);
    }
  }
  return parent;
}


/*
 * get int long value of variable environment.
 * If the variable is not set, return the default value 
 */
static long
getEnvLong(const char *env_var, long default_value)
{
  const char *str_val= getenv(env_var);

  if (str_val) {
    char *end_ptr;
    long val = strtol( str_val, &end_ptr, 10 );
    if ( end_ptr != env_var && (val < LONG_MAX && val > LONG_MIN) ) {
      return val;
    }
  }
  // invalid value
  return default_value;
}

/**
 * set precise_ip attribute to the max value
 *
 * TODO: this method only works on some platforms, and not
 *       general enough on all the platforms.
 */
static void
set_max_precise_ip(struct perf_event_attr *attr)
{
  // start with the most restrict skid (3) then 2, 1 and 0
  // this is specified in perf_event_open man page
  // if there's a change in the specification, we need to change
  // this one too (unfortunately)
  for(int i=perf_skid_flavors-1; i>=0; i--) {
	attr->precise_ip = perf_skid_precision[i];

	// ask sys to "create" the event
	// it returns -1 if it fails.
	int ret = perf_util_event_open(attr,
			THREAD_SELF, CPU_ANY,
			GROUP_FD, PERF_FLAGS);
	if (ret >= 0) {
	  close(ret);
	  // just quit when the returned value is correct
	  return;
	}
  }
}

//----------------------------------------------------------
// find the best precise ip value in this platform
// @param current perf event attribute. This attribute can be
//    updated for the default precise ip.
// @return the assigned precise ip 
//----------------------------------------------------------
static u64
get_precise_ip(struct perf_event_attr *attr)
{
  // check if user wants a specific ip-precision
  int val = getEnvLong(HPCRUN_OPTION_PRECISE_IP, PERF_EVENT_AUTODETECT_SKID);
  if (val >= PERF_EVENT_SKID_ARBITRARY && val <= PERF_EVENT_SKID_ZERO_REQUIRED)
  {
    attr->precise_ip = val;

    // check the validity of the requested precision
    // if it returns -1 we need to use our own auto-detect precision
    int ret = perf_util_event_open(attr,
            THREAD_SELF, CPU_ANY,
            GROUP_FD, PERF_FLAGS);
    if (ret >= 0) {
      return val;
    }
    EMSG("The kernel does not support the requested ip-precision: %d."
         " hpcrun will use auto-detect ip-precision instead.", val);
    set_max_precise_ip(attr);
  }
  return 0;
}


//----------------------------------------------------------
// predicates that test perf availability
//----------------------------------------------------------

int
perf_util_get_paranoid_level()
{
  FILE *pe_paranoid = fopen(LINUX_PERF_EVENTS_FILE, "r");
  int level         = 3; // default : no access to perf event

  if (pe_paranoid != NULL) {
    fscanf(pe_paranoid, "%d", &level) ;
  }
  if (pe_paranoid)
    fclose(pe_paranoid);

  return level;
}


//----------------------------------------------------------
// returns the maximum sample rate of this node
// based on info provided by LINUX_PERF_EVENTS_MAX_RATE file
//----------------------------------------------------------
int
perf_util_get_max_sample_rate()
{
  static int initialized = 0;
  static int max_sample_rate  = 300; // unless otherwise limited
  if (!initialized) {
    FILE *perf_rate_file = fopen(LINUX_PERF_EVENTS_MAX_RATE, "r");

    if (perf_rate_file != NULL) {
      fscanf(perf_rate_file, "%d", &max_sample_rate);
      fclose(perf_rate_file);
    }
    initialized = 1;
  }
  return max_sample_rate;
}


//----------------------------------------------------------
// testing perf availability
//----------------------------------------------------------
static int
perf_util_kernel_syms_avail()
{
  FILE *pe_paranoid = fopen(LINUX_PERF_EVENTS_FILE, "r");
  FILE *ksyms       = fopen(LINUX_KERNEL_SYMBOL_FILE, "r");

  int level         = 3; // default : no access to perf event

  if (ksyms != NULL && pe_paranoid != NULL) {
    fscanf(pe_paranoid, "%d", &level) ;
  }
  if (ksyms)       fclose(ksyms);
  if (pe_paranoid) fclose(pe_paranoid);

  return level;
}



/*************************************************************
 * Interface API
 **************************************************************/ 

//----------------------------------------------------------
// initialize perf_util. Need to be called as earliest as possible
//----------------------------------------------------------
void
perf_util_init()
{
  // perf_kernel_lm_id must be set for each process. here, we clear it 
  // because it is too early to allocate a load module. it will be set 
  // later, exactly once per process if ksym_status == PERF_AVAILABLE.
  perf_kernel_lm_id = 0; 

  // if kernel symbols are available, we will attempt to collect kernel
  // callchains and add them to our call paths

  ksym_status = PERF_UNAVAILABLE;

  // Conditions for the sample to include kernel if:
  // 1. kptr_restric   = 0    (zero)
  // 2. paranoid_level < 2    (zero or one)
  // 3. linux version  > 3.7

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
  int level     = perf_util_kernel_syms_avail();
  int krestrict = perf_util_get_kptr_restrict();

  if (krestrict == 0 && (level == 0 || level == 1)) {
    hpcrun_kernel_callpath_register(perf_add_kernel_callchain);
    ksym_status = PERF_AVAILABLE;
  }
#endif
}


//----------------------------------------------------------
// Interface to see if the kernel symbol is available
// this function caches the value so that we don't need
//   enquiry the same question all the time.
//----------------------------------------------------------
bool
perf_util_is_ksym_available()
{
  return (ksym_status == PERF_AVAILABLE);
}


//----------------------------------------------------------
// create a new event
//----------------------------------------------------------
long
perf_util_event_open(struct perf_event_attr *hw_event, pid_t pid,
         int cpu, int group_fd, unsigned long flags)
{
   int ret;

   ret = syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
   return ret;
}

//----------------------------------------------------------
// generic default initialization for event attributes
// return true if the initialization is successful,
//   false otherwise.
//----------------------------------------------------------
int
perf_util_attr_init(
  struct perf_event_attr *attr,
  bool usePeriod, u64 threshold,
  u64  sampletype
)
{
  // by default, we always ask for sampling period information
  // some PMUs is sensitive to the sample type.
  // For instance, IDLE-CYCLES-BACKEND will fail if we set PERF_SAMPLE_ADDR.
  // By default, we need to initialize sample_type as minimal as possible.
  unsigned int sample_type = sampletype 
                             | PERF_SAMPLE_PERIOD | PERF_SAMPLE_TIME;

  attr->size   = sizeof(struct perf_event_attr); /* Size of attribute structure */
  attr->freq   = (usePeriod ? 0 : 1);

  attr->precise_ip    = get_precise_ip(attr);   /* the precision is either detected automatically
                                              as precise as possible or  on the user's variable.  */

  attr->sample_period = threshold;          /* Period or frequency of sampling     */
  int max_sample_rate = perf_util_get_max_sample_rate();

  if (attr->freq == 1 && threshold >= max_sample_rate) {
    int our_rate = max_sample_rate - 1;
    EMSG("WARNING: Lowered specified sample rate %d to %d, below max sample rate of %d.",
          threshold, our_rate, max_sample_rate);
    attr->sample_period = our_rate;
  }

  attr->disabled      = 1;                 /* the counter will be enabled later  */
  attr->sample_type   = sample_type;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
  attr->exclude_callchain_user   = EXCLUDE_CALLCHAIN;
  attr->exclude_callchain_kernel = EXCLUDE_CALLCHAIN;
#endif

  attr->exclude_kernel = EXCLUDE;
  attr->exclude_hv     = EXCLUDE;

  if (perf_util_is_ksym_available()) {
    /* We have rights to record and interpret kernel callchains */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
    attr->sample_type             |= PERF_SAMPLE_CALLCHAIN;
    attr->exclude_callchain_kernel = INCLUDE_CALLCHAIN;
#endif
    attr->exclude_kernel           = INCLUDE;
  }

  return true;
}

