// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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

#include "debug-flag.h"
#include "fmt.h"
#include "errors.h"

//*****************************************************************************
// macros
//*****************************************************************************

#define EMSG           hpcrun_emsg
#define AMSG           hpcrun_amsg

#ifdef COMPILE_IN
#define CTMSG(...) TMSG(__VA_ARGS__)
#else
#define CTMSG(...)
#endif // COMPILE_IN

#ifdef NO_HPCRUN_MSGS
#define STDERR_MSG(...)
#define EEMSG(...)
#define PMSG(f,...)
#define TMSG(f,...)
#define ETMSG(f,...)
#define NMSG(f,...)
#define ENMSG(f, ...)
#else // ! NO_HPCRUN_MSGS

#define STDERR_MSG(...) hpcrun_stderr_log_msg(false,__VA_ARGS__)
#define EEMSG(...)      hpcrun_stderr_log_msg(true,__VA_ARGS__)

#define PMSG(f,...)    hpcrun_pmsg(DBG_PREFIX(f), NULL, __VA_ARGS__)
#define TMSG(f,...) if (debug_flag_get(DBG_PREFIX(f))) hpcrun_pmsg(#f, __VA_ARGS__)

#define ETMSG(f,...)   hpcrun_pmsg_stderr(true,DBG_PREFIX(f), #f, __VA_ARGS__)
#define NMSG(f,...) if (debug_flag_get(DBG_PREFIX(f))) hpcrun_nmsg(DBG_PREFIX(f), #f, __VA_ARGS__)
#define ENMSG(f, ...)  hpcrun_nmsg_stderr(true, DBG_PREFIX(f), #f, __VA_ARGS__)
#endif // NO_HPCRUN_MSGS

#define EXIT_ON_ERROR(r,e,...) hpcrun_exit_on_error(r,e,__VA_ARGS__)

#define hpcrun_abort(...) hpcrun_abort_w_info(messages_donothing, __VA_ARGS__)



void messages_init();
void messages_fini(void);

void messages_logfile_create();
int  messages_logfile_fd(void);

void messages_donothing(void);

void hpcrun_amsg(const char *fmt,...);
void hpcrun_emsg(const char *fmt,...);
void hpcrun_emsg_valist(const char *fmt, va_list_box* box);
void hpcrun_nmsg(pmsg_category flag, const char* tag, const char *fmt,...);
extern void hpcrun_pmsg(const char* tag, const char *fmt,...);

void hpcrun_pmsg_stderr(bool echo_stderr,pmsg_category flag, const char* tag,
                        const char *fmt,...);
void hpcrun_nmsg_stderr(bool echo_stderr,pmsg_category flag, const char* tag,
                        const char *fmt,...);
void hpcrun_stderr_log_msg(bool copy_to_log, const char *fmt,...);
void hpcrun_exit_on_error(int ret, int ret_expected, const char *fmt,...);

void hpcrun_abort_w_info(void (*info)(void),const char *fmt,...);

int hpcrun_below_pmsg_threshold(void);
void hpcrun_up_pmsg_count(void);

void unlimit_msgs(void);
void limit_msgs(void);

#endif // messages_h
