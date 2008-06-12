//===============================================
// File: system_server.c  
// 
//     use an extra "server" process to handle computing the
//     symbols to insulate the client process from the complexity of this task, 
//     including use of the system command to fork new processes. having the
//     server process enables use to avoid dealing with forking off new processes
//     with system when there might be multiple threads active with sampling  
//     enabled.
//
//  Modification history:
//     2008 April 29 - created John Mellor-Crummey
//===============================================

//*******************************************************************************
// system includes 
//*******************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // for strlen
#include <stdarg.h> // for varargs
#include <unistd.h>  //for pipe



//*******************************************************************************
// local includes 
//*******************************************************************************

//-------------
// monitor
//-------------
#include "monitor.h"

#include "pmsg.h"
#include "system_server.h"


#if 0
//*******************************************************************************
// forward declarations
//*******************************************************************************

static void exit_on_error(int ret, int ret_expected, char *format, ...);
#endif


//*******************************************************************************
// private data  
//*******************************************************************************

// constant data 
static const int server_done = -1;

static int fnbounds_server_pipes[2][2];
static pid_t server_pid;


//*******************************************************************************
// macros 
//*******************************************************************************

#define CLIENT_TO_SERVER 0
#define SERVER_TO_CLIENT 1


#define READ_FD(end) fnbounds_server_pipes[end][0]
#define WRITE_FD(end) fnbounds_server_pipes[end][1]



//*******************************************************************************
// interface operations  
//*******************************************************************************

int 
system_server_start()
{
  int ret;
  extern int __libc_system(char *);

  // --------------------------------------------------------------------------------------------------
  // set up pipes for communication between client and server
  // --------------------------------------------------------------------------------------------------
  ret = pipe(fnbounds_server_pipes[CLIENT_TO_SERVER]);
  EXIT_ON_ERROR(ret, 0, "fnbounds server: failed to create 1st pipe");

  ret = pipe(fnbounds_server_pipes[SERVER_TO_CLIENT]);
  EXIT_ON_ERROR(ret, 0, "fnbounds server: failed to create 2nd pipe");

  // --------------------------------------------------------------------------------------------------
  // fork server from client
  // --------------------------------------------------------------------------------------------------
  server_pid = monitor_real_fork();
  if (server_pid == 0) { /* child process */
     NMSG(SYSTEM_SERVER,"in child");

     // --------------------------------------------------------------------------------------------------
     // avoid monitoring processes that we spawn with system
     // --------------------------------------------------------------------------------------------------
     unsetenv("LD_PRELOAD");

     // --------------------------------------------------------------------------------------------------
     // server: close unused file descriptor to send data to self 
     // --------------------------------------------------------------------------------------------------
     ret = close(WRITE_FD(CLIENT_TO_SERVER));
     EXIT_ON_ERROR(ret, 0, "fnbounds server: failed to close pipe in server");

     for(;;) {
       int nbytes;

       // --------------------------------------------------------------------------------------------------
       // receive command string length from client
       // --------------------------------------------------------------------------------------------------
       ret = read(READ_FD(CLIENT_TO_SERVER), &nbytes, sizeof(nbytes));
       EXIT_ON_ERROR(ret, sizeof(nbytes), "fnbounds server fatal error: read failed");

       if (nbytes > 0) {
          TMSG(SYSTEM_SERVER,"fnbounds server: expecting command of %d bytes", nbytes);

	  // --------------------------------------------------------------------------------------------------
	  // allocate space for command string 
	  // --------------------------------------------------------------------------------------------------
          char *command = malloc(nbytes);

	  // --------------------------------------------------------------------------------------------------
	  // receive command string from client
	  // --------------------------------------------------------------------------------------------------
          ret = read(READ_FD(CLIENT_TO_SERVER), command, nbytes);
          EXIT_ON_ERROR(ret, nbytes, "fnbounds server fatal error: "
	                "read only %d of %d bytes for a command %d", ret, nbytes);

	  // --------------------------------------------------------------------------------------------------
	  // execute command 
	  //
	  // NOTE: must use monitor_real_system to avoid monitor callbacks 
	  //             for fork and exec as part of system because monitor has 
	  //             control of server process 
	  // --------------------------------------------------------------------------------------------------
	  TMSG(SYSTEM_SERVER,"executing command: '%s'\n", command);
          int status = monitor_real_system(command);

	  // --------------------------------------------------------------------------------------------------
	  // send command result to client
	  // --------------------------------------------------------------------------------------------------
          ret = write(WRITE_FD(SERVER_TO_CLIENT), &status, sizeof(status));

	  // --------------------------------------------------------------------------------------------------
	  // free space for command string 
	  // --------------------------------------------------------------------------------------------------
	  free(command);
       } else {
	 if (nbytes != server_done) {
            EXIT_ON_ERROR(nbytes, server_done, "fnbounds server fatal error: "
			  "received negative length %d for command string", nbytes);
	 }
	 // --------------------------------------------------------------------------------------------------
	 // received command string length value server_done to terminate 
	 //
	 // NOTE: must use monitor_exit to avoid monitor callbacks on exit
	 //             because monitor has control of server process 
	 // --------------------------------------------------------------------------------------------------
         monitor_real_exit(0); 
       }
     }
  } 

  // --------------------------------------------------------------------------------------------------
  // client: close unused file descriptor to send data to self 
  // --------------------------------------------------------------------------------------------------
  ret = close(WRITE_FD(SERVER_TO_CLIENT));
  EXIT_ON_ERROR(ret, 0, "fnbounds server: failed to close pipe in client");

  return (server_pid != -1) ? 0 : -1;
}


void 
system_server_shutdown() 
{
  // --------------------------------------------------------------------------------------------------
  // send command string length value of -1 to terminate 
  // --------------------------------------------------------------------------------------------------
  int ret = write(WRITE_FD(CLIENT_TO_SERVER), &server_done, sizeof(server_done));
  EXIT_ON_ERROR(ret, sizeof(server_done), "fnbounds server shutdown failed");
}
 

int 
system_server_execute_command(char *command)  
{
  int ret, status;
  int len = strlen(command) + 1;         // add 1 for terminating null

  // --------------------------------------------------------------------------------------------------
  // send command string length to server
  // --------------------------------------------------------------------------------------------------
  TMSG(SYSTEM_COMMAND,"fnbounds server: sending command size = %d bytes", len);
  ret = write(WRITE_FD(CLIENT_TO_SERVER), &len, sizeof(len));
  EXIT_ON_ERROR(ret, sizeof(len), "fnbounds server fatal error: write of command size failed");

  // --------------------------------------------------------------------------------------------------
  // send command string to server
  // --------------------------------------------------------------------------------------------------
  ret = write(WRITE_FD(CLIENT_TO_SERVER), command, len);
  EXIT_ON_ERROR(ret, len, "fnbounds server fatal error: write command failed");

  // --------------------------------------------------------------------------------------------------
  // receive command result from server 
  // --------------------------------------------------------------------------------------------------
  ret = read(READ_FD(SERVER_TO_CLIENT), &status, sizeof(status));
  EXIT_ON_ERROR(ret, sizeof(status), "fnbounds server fatal error: read status failed\n");

  return status;
}

int system(__const char *command) 
{
  return system_server_execute_command(command);  
}
