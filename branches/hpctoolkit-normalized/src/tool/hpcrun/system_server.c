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
// Copyright ((c)) 2002-2010, Rice University 
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

//******************************************************************************
// File: system_server.c  
// 
// Description:
//   use an extra "server" process to handle computing the
//   symbols to insulate the client process from the complexity of this task, 
//   including use of the system command to fork new processes. having the
//   server process enables use to avoid dealing with forking off new 
//   processes with system when there might be multiple threads active with 
//   sampling enabled.
//
//  Modification history:
//     2008 April 29 - John Mellor-Crummey - created
//     2009 July 19  - John Mellor-Crummey - clean up formatting
//******************************************************************************



//******************************************************************************
// system includes 
//******************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for strlen
#include <stdarg.h> // for varargs
#include <unistd.h> // for pipe



//******************************************************************************
// local includes 
//******************************************************************************

//-------------
// monitor
//-------------
#include "monitor.h"

#include "messages.h"
#include "system_server.h"



//******************************************************************************
// private data  
//******************************************************************************

// constant data 
static const int server_done = -1;

static int system_server_pipes[2][2];
static pid_t server_pid;



//******************************************************************************
// macros 
//******************************************************************************

#define CLIENT_TO_SERVER 0
#define SERVER_TO_CLIENT 1

#define READ_FD(end) system_server_pipes[end][0]
#define WRITE_FD(end) system_server_pipes[end][1]

#define SS_EXIT_ON_ERROR(r,e,...) system_server_exit_on_error(r,e,__VA_ARGS__)



//******************************************************************************
// forward declarations 
//******************************************************************************

void system_server_exit_on_error(int ret, int ret_expected, const char *fmt, 
				 ...);


//******************************************************************************
// interface operations  
//******************************************************************************

int 
system_server_start()
{
  int ret;
  extern int __libc_system(char *);

  // ---------------------------------------------------------------------------
  // set up pipes for communication between client and server
  // ---------------------------------------------------------------------------
  ret = pipe(system_server_pipes[CLIENT_TO_SERVER]);
  EXIT_ON_ERROR(ret, 0, "system server: failed to create 1st pipe");

  ret = pipe(system_server_pipes[SERVER_TO_CLIENT]);
  EXIT_ON_ERROR(ret, 0, "system server: failed to create 2nd pipe");

  // ---------------------------------------------------------------------------
  // fork server from client
  // ---------------------------------------------------------------------------
  server_pid = monitor_real_fork();
  if (server_pid == 0) { /* child process */
     NMSG(SYSTEM_SERVER,"in child");

     // -----------------------------------------------------------------------
     // avoid monitoring processes that we spawn with system
     // -----------------------------------------------------------------------
     unsetenv("LD_PRELOAD");

     // -----------------------------------------------------------------------
     // server: close unused file descriptor to send data to self 
     // -----------------------------------------------------------------------
     ret = close(WRITE_FD(CLIENT_TO_SERVER));
     SS_EXIT_ON_ERROR(ret, 0, "system server: failed to close pipe in server");

     for(;;) {
       int nbytes;

       // ----------------------------------------------------------------------
       // receive command string length from client
       // ----------------------------------------------------------------------
       ret = read(READ_FD(CLIENT_TO_SERVER), &nbytes, sizeof(nbytes));
       SS_EXIT_ON_ERROR(ret, sizeof(nbytes), 
			"system server fatal error: read failed");

       if (nbytes > 0) {
          TMSG(SYSTEM_SERVER, "system server: expecting command of %d bytes", 
	       nbytes);

	  // ------------------------------------------------------------------
	  // allocate space for command string 
	  // ------------------------------------------------------------------
          char *command = malloc(nbytes);

	  // ------------------------------------------------------------------
	  // receive command string from client
	  // ------------------------------------------------------------------
          ret = read(READ_FD(CLIENT_TO_SERVER), command, nbytes);
          SS_EXIT_ON_ERROR(ret, nbytes, "system server fatal error: "
			   "read only %d of %d bytes for a command %d", ret, 
			   nbytes);

	  // -------------------------------------------------------------------
	  // execute command 
	  //
	  // NOTE: must use monitor_real_system to avoid monitor callbacks 
	  //       for fork and exec as part of system because monitor has 
	  //       control of server process 
	  // -------------------------------------------------------------------
	  TMSG(SYSTEM_SERVER,"executing command: '%s'\n", command);
          int status = monitor_real_system(command);

	  // -------------------------------------------------------------------
	  // send command result to client
	  // -------------------------------------------------------------------
          ret = write(WRITE_FD(SERVER_TO_CLIENT), &status, sizeof(status));

	  // -------------------------------------------------------------------
	  // free space for command string 
	  // -------------------------------------------------------------------
	  free(command);
       } else {
	 if (nbytes != server_done) {
            SS_EXIT_ON_ERROR(nbytes, server_done, "system server fatal error: "
			     "received negative length %d for command string", 
			     nbytes);
	 }
	 // --------------------------------------------------------------------
	 // received command string length value server_done to terminate 
	 //
	 // NOTE: must use monitor_exit to avoid monitor callbacks on exit
	 //             because monitor has control of server process 
	 // --------------------------------------------------------------------
         monitor_real_exit(0); 
       }
     }
  } 

  // ---------------------------------------------------------------------------
  // client: close unused file descriptor to send data to self 
  // ---------------------------------------------------------------------------
  ret = close(WRITE_FD(SERVER_TO_CLIENT));
  EXIT_ON_ERROR(ret, 0, "system server: failed to close pipe in client");

  return (server_pid != -1) ? 0 : -1;
}


void 
system_server_shutdown() 
{
  // ---------------------------------------------------------------------------
  // send command string length value of -1 to terminate 
  // but not if we're called back in the server (child) itself
  // ---------------------------------------------------------------------------
  if (server_pid != 0) {
    int ret = write(WRITE_FD(CLIENT_TO_SERVER), &server_done, 
		    sizeof(server_done));
    EXIT_ON_ERROR(ret, sizeof(server_done), "system server shutdown failed");
  }
}
 

int 
system_server_execute_command(const char *command)  
{
  int ret, status;
  int len = strlen(command) + 1;         // add 1 for terminating null

  // ---------------------------------------------------------------------------
  // send command string length to server
  // ---------------------------------------------------------------------------
  TMSG(SYSTEM_COMMAND,"system server: sending command size = %d bytes", len);
  ret = write(WRITE_FD(CLIENT_TO_SERVER), &len, sizeof(len));
  EXIT_ON_ERROR(ret, sizeof(len), 
		"system server fatal error: write of command size failed");

  // ---------------------------------------------------------------------------
  // send command string to server
  // ---------------------------------------------------------------------------
  ret = write(WRITE_FD(CLIENT_TO_SERVER), command, len);
  EXIT_ON_ERROR(ret, len, "system server fatal error: write command failed");

  // ---------------------------------------------------------------------------
  // receive command result from server 
  // ---------------------------------------------------------------------------
  ret = read(READ_FD(SERVER_TO_CLIENT), &status, sizeof(status));
  EXIT_ON_ERROR(ret, sizeof(status), 
		"system server fatal error: read status failed\n");

  return status;
}


int 
system(__const char *command) 
{
  return system_server_execute_command(command);  
}


void
system_server_exit_on_error(int ret, int ret_expected, const char *fmt, ...)
{
  if (ret == ret_expected) {
    return;
  }
  va_list args;
  va_start(args,fmt);
  hpcrun_emsg(fmt, args); 
  monitor_real_exit(0);
}
