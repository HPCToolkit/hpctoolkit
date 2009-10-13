// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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

#ifndef messages_h
#define messages_h

//*****************************************************************************
// global includes
//*****************************************************************************
#include <stdarg.h>
#include <stdbool.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include <messages/debug-flag.h>
#include <messages/fmt.h>


//*****************************************************************************
// macros
//*****************************************************************************

#define PMSG_LIMIT(C) if (hpcrun_below_pmsg_threshold()) C

#define STDERR_MSG(...) hpcrun_stderr_log_msg(false,__VA_ARGS__)
#define EMSG            hpcrun_emsg
#define EEMSG(...)      hpcrun_stderr_log_msg(true,__VA_ARGS__)

#define AMSG           hpcrun_amsg
#define PMSG(f,...)    hpcrun_pmsg(DBG_PREFIX(f), NULL, __VA_ARGS__)
#define TMSG(f,...)    hpcrun_pmsg(DBG_PREFIX(f), #f, __VA_ARGS__)
#define ETMSG(f,...)   hpcrun_pmsg_stderr(true,DBG_PREFIX(f), #f, __VA_ARGS__)
#define NMSG(f,...)    hpcrun_nmsg(DBG_PREFIX(f), #f, __VA_ARGS__)
#define ENMSG(f, ...)  hpcrun_nmsg_stderr(true, DBG_PREFIX(f), #f, __VA_ARGS__)

#define EXIT_ON_ERROR(r,e,...) hpcrun_exit_on_error(r,e,__VA_ARGS__)

#define hpcrun_abort(...) hpcrun_abort_w_info(messages_donothing, __VA_ARGS__)



//*****************************************************************************
// forward declarations
//*****************************************************************************

void messages_init();
void messages_fini(void);

void messages_logfile_create();
int  messages_logfile_fd(void);

void messages_donothing(void);

void hpcrun_amsg(const char *fmt,...);
void hpcrun_emsg(const char *fmt,...);
void hpcrun_emsg_valist(const char *fmt, va_list_box* box);
void hpcrun_nmsg(pmsg_category flag, const char* tag, const char *fmt,...);
void hpcrun_pmsg(pmsg_category flag, const char* tag, const char *fmt,...);

void hpcrun_pmsg_stderr(bool echo_stderr,pmsg_category flag, const char* tag, 
			const char *fmt,...);
void hpcrun_nmsg_stderr(bool echo_stderr,pmsg_category flag, const char* tag, 
			const char *fmt,...);
void hpcrun_stderr_log_msg(bool copy_to_log, const char *fmt,...);
void hpcrun_exit_on_error(int ret, int ret_expected, const char *fmt,...);

void hpcrun_abort_w_info(void (*info)(void),const char *fmt,...);

int hpcrun_below_pmsg_threshold(void);
void hpcrun_up_pmsg_count(void);



#endif // messages_h
