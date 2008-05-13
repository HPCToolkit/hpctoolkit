//===============================================
// File: system_server.h  
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
// interface operations  
//*******************************************************************************

int system_server_start();


void system_server_shutdown();
 

int system_server_execute_command(char *command);  
