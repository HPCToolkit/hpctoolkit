//===============================================
// File: fnbounds_dynamic_server.h  
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

int fnbounds_dynamic_server_start();


void fnbounds_dynamic_server_shutdown();
 

int fnbounds_dynamic_server_execute_command(char *command);  

// Aliases for convenience

#define fnbounds_init fnbounds_dynamic_server_start
#define fnbounds_fini fnbounds_dynamic_server_shutdown

extern int csprof_child_process;
