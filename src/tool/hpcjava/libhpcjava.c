// -*-Mode: C++;-*- // technically C99

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
// Copyright ((c)) 2002-2012, Rice University
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

#include <stdio.h>
#include <stdbool.h> // standard boolean
#include <jvmti.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>  // PATH_MAX

#include <sys/types.h>
#include <sys/time.h>

// rank and thread information 
#include "thread_data.h"
#include "rank.h"

// hpcrun specific headers
#include "files.h"
#include <fnbounds/fnbounds_java.h>
#include "safe-sampling.h"
#include "disabled.h"

// java and oprofile headers
#include "opagent.h"
#include "java/jmt.h"
#include "java_callstack.h"
#include "stacktraces.h"

// monitor's real functions
#include <monitor.h>

/**
 * Name of the environment variable that stores the absolute path of opjitconv
 * This variable should be set by hpcrun script (or manually)
 **/
#define OPROFILE_CMD_ENV "HPCRUN_OPJITCONV_CMD"

/**
 * Flag whether we want to fork the execution of opjitconv or not
 * For debugging purpose, we may need to avoid the fork 
 **/
#define HPCJAVA_FORK_OPJITCONV 1

/**
 * Macro to  control the generation of events
 * This macro is designed to be called by setup_callbacks() function only
 **/
#define JAVA_REGISTER_EVENT(Event) 		\
  error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, 	\
					     (Event), NULL); 		\
  if (handle_error(error, "SetEventNotificationMode() " 		\
		   #Event, 1)) { 					\
    return false;							\
  }
	
//*****************************************************************
// global variables
//*****************************************************************

static int debug = 0;
static int can_get_line_numbers = 1;

static struct timeval time_start;
static char file_dump[PATH_MAX] = {'\0'};

static op_agent_t agent_hdl;
static JavaVM *java_vm;
static jvmtiEnv * jvmti = NULL;

extern char **environ;

//*****************************************************************
// helper functions
//*****************************************************************

/**
 * Handle an error or a warning, return 0 if the checked error is 
 * JVMTI_ERROR_NONE, i.e. success
 */
static int handle_error(jvmtiError err, char const * msg, int severe)
{
  if (err != JVMTI_ERROR_NONE) {
    fprintf(stderr, "%s: %s, err code %i\n",
	    severe ? "Error" : "Warning", msg, err);
  }
  return err != JVMTI_ERROR_NONE;
}


/**
 * create debug information (if exist) from a given java map 
 *
 */
static struct debug_line_info *
create_debug_line_info(jint map_length, jvmtiAddrLocationMap const * map,
		       jint entry_count, jvmtiLineNumberEntry* table_ptr,
		       char const * source_filename)
{
  struct debug_line_info * debug_line;
  int i, j;
  if (debug) {
    fprintf(stderr, "Source %s, l: %d, addr: %p - %p\n", source_filename, map_length, 
	    map[0].start_address, map[map_length-1].start_address);
  }

  debug_line = calloc(map_length, sizeof(struct debug_line_info));
  if (!debug_line)
    return 0;

  for (i = 0; i < map_length; ++i) {
    /* FIXME: likely to need a lower_bound on the array, but
     * documentation is a bit obscure about the contents of these
     * arrray
     **/
    for (j = 0; j < entry_count - 1; ++j) {
      if (table_ptr[j].start_location > map[i].location)
	break;
    }
    debug_line[i].vma = (unsigned long)map[i].start_address;
    debug_line[i].lineno = table_ptr[j].line_number;
    debug_line[i].filename = source_filename;
  }

  return debug_line;
}

//*****************************************************************
// callback functions
//*****************************************************************


/**
 * A class load event is generated when a class is first loaded. 
 * The order of class load events generated by a particular thread are
 *  guaranteed to match the order of class loading within that thread. 
 *  Array class creation does not generate a class load event. 
 *  The creation of a primitive class (for example, java.lang.Integer.TYPE) does not generate a class load event.
 *
 * This event is sent at an early stage in loading the class.
 * As a result the class should be used carefully.
 * Note, for example, that methods and fields are not yet loaded,
 * so queries for methods, fields, subclasses, and so on will not give correct results. 
 */
static void JNICALL
cb_class_load(jvmtiEnv *_jvmti_env,
	      JNIEnv* _jni_env,
	      jthread _thread,
	      jclass _klass)
{
  /************* hpcrun critical section ************/
  if (!hpcrun_safe_enter()) {
    return ;
  }
	
  /************* end hpcrun critical section ************/
  hpcrun_safe_exit();
}

/**
 * A class prepare event is generated when class preparation is complete. 
 * At this point, class fields, methods, and implemented interfaces are available, 
 * and no code from the class has been executed. 
 *
 * Since array classes never have fields or methods, class prepare events are not generated for them. 
 * Class prepare events are not generated for primitive classes (for example, java.lang.Integer.TYPE).
 */
static void JNICALL
cb_class_prepare(jvmtiEnv *je,
		 JNIEnv* jni_env,
		 jthread thread,
		 jclass klass)
{
  /************* hpcrun critical section ************/
  if (!hpcrun_safe_enter()) {
    return ;
  }

  // We need to do this to "prime the pump",
  // as it were -- make sure that all of the
  // methodIDs have been initialized internally,
  // for AsyncGetCallTrace.  I imagine it slows
  // down class loading a mite, but honestly,
  // how fast does class loading have to be?

  jint 	     method_count;
  jmethodID  *methods;
  jvmtiError e = (*je)->GetClassMethods(je, klass, &method_count, &methods);

  if (e != JVMTI_ERROR_NONE && e != JVMTI_ERROR_CLASS_NOT_PREPARED) {

    // JVMTI_ERROR_CLASS_NOT_PREPARED is okay because some classes may
    // be loaded but not prepared at this point.

    char *ksig;
    e = (*je)->GetClassSignature(je, klass, &ksig, NULL);

    if (e != JVMTI_ERROR_NONE) {
 	fprintf(stderr, "%d Failed to create method ID for %s\n", e, ksig);
    }
    (*je)->Deallocate(je, (unsigned char*) ksig);
  }
  (*je)->Deallocate(je, (unsigned char*) methods);
	
  /************* end hpcrun critical section ************/
  hpcrun_safe_exit();
}

/*
 * Sent when a method is compiled and loaded into memory by the VM. 
 * If it is unloaded, the CompiledMethodUnload event is sent. 
 * If it is moved, the CompiledMethodUnload event is sent, followed by a new CompiledMethodLoad event. 
 *
 * Note that a single method may have multiple compiled forms, and that this event will be sent for each form. 
 * Note also that several methods may be inlined into a single address range, 
 * and that this event will be sent for each method. 
 * 
 */
static void JNICALL 
cb_compiled_method_load(jvmtiEnv * jvmti,
			jmethodID method, jint code_size, void const * code_addr,
			jint map_length, jvmtiAddrLocationMap const * map,
			void const * compile_info)
{
  jclass declaring_class;
  char * class_signature = NULL;
  char * method_name = NULL;
  char * method_signature = NULL;
  jvmtiLineNumberEntry* table_ptr = NULL;
  char * source_filename = NULL;
  struct debug_line_info * debug_line = NULL;
  jvmtiError err;

  /************* hpcrun critical section ************/
  if (!hpcrun_safe_enter()) {
    return;
  }

  jmt_add_java_method(method, code_addr);

  /* 
   * check if we can access to functions in hpcrun.so
   *  if dlopen fails or the function is not found, we just do nothing
   *  
   * */

  /* shut up compiler warning */
  compile_info = compile_info;

  err = (*jvmti)->GetMethodDeclaringClass(jvmti, method,
					  &declaring_class);
  if (handle_error(err, "GetMethodDeclaringClass()", 1))
    goto cleanup2;

  if (can_get_line_numbers && map_length && map) {
    jint entry_count;

    err = (*jvmti)->GetLineNumberTable(jvmti, method,
				       &entry_count, &table_ptr);
    if (err == JVMTI_ERROR_NONE) {
      err = (*jvmti)->GetSourceFileName(jvmti,
					declaring_class, &source_filename);
      if (err ==  JVMTI_ERROR_NONE) {
	debug_line =
	  create_debug_line_info(map_length, map,
				 entry_count, table_ptr,
				 source_filename);
			 
      } else if (err != JVMTI_ERROR_ABSENT_INFORMATION) {
	handle_error(err, "GetSourceFileName()", 1);
      }
    } else if (err != JVMTI_ERROR_NATIVE_METHOD &&
	       err != JVMTI_ERROR_ABSENT_INFORMATION) {
      handle_error(err, "GetLineNumberTable()", 1);
    }
  }

  err = (*jvmti)->GetClassSignature(jvmti, declaring_class,
				    &class_signature, NULL);
  if (handle_error(err, "GetClassSignature()", 1))
    goto cleanup1;

  err = (*jvmti)->GetMethodName(jvmti, method, &method_name,
				&method_signature, NULL);
  if (handle_error(err, "GetMethodName()", 1))
    goto cleanup;

  if (debug) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL)) {
      fprintf(stderr,"gettimeofday fail");
    }
    else
      fprintf(stderr, "load: t=%ld, declaring_class=%p, class=%s, "
	    "method=%s, signature=%s, addr=%p, size=%i \n",  tv.tv_usec,
	    declaring_class, class_signature, method_name,
	    method_signature, code_addr, code_size);
  }

  {
    int cnt = strlen(method_name) + strlen(class_signature) +
      strlen(method_signature) + 2;
    char buf[cnt];
    strncpy(buf, class_signature, cnt - 1);
    strncat(buf, method_name, cnt - strlen(buf) - 1);
    strncat(buf, method_signature, cnt - strlen(buf) - 1);
    const void *code_addr_end = code_addr + code_size;

    hpcjava_add_address_interval(code_addr, code_addr_end);

    if (op_write_native_code(agent_hdl, buf,
			     (uint64_t)(uintptr_t) code_addr,
			     code_addr, code_size)) {
      perror("Error: op_write_native_code()");
      goto cleanup;
    }
  }

  if (debug_line)
    if (op_write_debug_line_info(agent_hdl, code_addr, map_length,
				 debug_line))
      perror("Error: op_write_debug_line_info()");

 cleanup:
  (*jvmti)->Deallocate(jvmti, (unsigned char *)method_name);
  (*jvmti)->Deallocate(jvmti, (unsigned char *)method_signature);
 cleanup1:
  (*jvmti)->Deallocate(jvmti, (unsigned char *)class_signature);
  (*jvmti)->Deallocate(jvmti, (unsigned char *)table_ptr);
  (*jvmti)->Deallocate(jvmti, (unsigned char *)source_filename);
 cleanup2:
  free(debug_line);
	
  /************* end hpcrun critical section ************/
  hpcrun_safe_exit();
}


/**
 * Sent when a compiled method is unloaded from memory. 
 * This event might not be sent on the thread which performed the unload. 
 * This event may be sent sometime after the unload occurs, 
 * 	but will be sent before the memory is reused by a newly generated compiled method. 
 * This event may be sent after the class is unloaded. 
 */
static void JNICALL cb_compiled_method_unload(jvmtiEnv * jvmti_env,
					      jmethodID method, void const * code_addr)
{
  /************* hpcrun critical section ************/
  if (!hpcrun_safe_enter()) {
    return ;
  }

  /* shut up compiler warning */
  jvmti_env = jvmti_env;
  method = method;

  if (debug)
    fprintf(stderr, "unload: addr=%p\n", code_addr);
  if (op_unload_native_code(agent_hdl, (uint64_t)(uintptr_t) code_addr))
    perror("Error: op_unload_native_code()");

  //hpcjava_detach_thread();

  /************* end hpcrun critical section ************/
  hpcrun_safe_exit();
}

/**
 * Sent when a component of the virtual machine is generated dynamically. 
 * This does not correspond to Java programming language code that is compiled--see CompiledMethodLoad. 
 * This is for native code--for example, an interpreter that is generated differently 
 * 	depending on command-line options.
 *
 * Note that this event has no controlling capability. 
 * If a VM cannot generate these events, it simply does not send any.
 *
 * These events can be sent after their initial occurrence with GenerateEvents. 
 */
static void JNICALL 
cb_dynamic_code_generated(jvmtiEnv * jvmti_env,
		      char const * name, void const * code_addr, jint code_size)
{
  /************* hpcrun critical section ************/
  if (!hpcrun_safe_enter()) {
    return ;
  }

  /* shut up compiler warning */
  jvmti_env = jvmti_env;

  /* adding the interval to the java ui tree
   */ 	 
  const void *code_addr_end = code_addr + code_size;
  hpcjava_add_address_interval(code_addr, code_addr_end);

  if (op_write_native_code(agent_hdl, name,
			   (uint64_t)(uintptr_t) code_addr,
			   code_addr, code_size))
    perror("Error: op_write_native_code()");
	
  /************* end hpcrun critical section ************/
  hpcrun_safe_exit();
}


/***
 * Called before Java's termination
 * 
 * It will call opjitconv to convert the dumped Java's compiled code
 *  into a .so file (named .jo file)
 */
static void
libhpcjava_fini()
{
  char end_time_str[32], start_time_str[32];
  char * exec_args[8];
  int arg_num;
  char *jitconv_pgm = "opjitconv";

  struct timeval time_end;

#if HPCJAVA_FORK_OPJITCONV
  pid_t childpid;

  childpid = monitor_real_fork();
  if (childpid == 0) {
    //
    //  child process: disable profiling, run opjitconv
    //         
#endif
    hpcrun_set_disabled();

    // opjitconv requires the range of the time
    // so we provide the time from VM Load and VM Unload
    gettimeofday(&time_end, NULL);
    sprintf(end_time_str, "%ld", time_end.tv_sec);
    sprintf(start_time_str, "%ld", time_start.tv_sec);

    if (debug)
      printf("Time range jvmti: %ld - %ld \n", time_start.tv_sec, time_end.tv_sec );

    char *oprofile_cmd = getenv(OPROFILE_CMD_ENV);
    if (oprofile_cmd == NULL) {
      fprintf(stderr, "Error: Env variable %s is not set\n", OPROFILE_CMD_ENV);
    } else {
      arg_num = 0;
      exec_args[arg_num++] = (char *) jitconv_pgm;
      exec_args[arg_num++] = (char *) file_dump;
      exec_args[arg_num++] = start_time_str;
      exec_args[arg_num++] = end_time_str;
      exec_args[arg_num] = (char *) NULL;

      if (debug)
	printf("executing %s .... \n", oprofile_cmd);

      monitor_real_execve(oprofile_cmd, exec_args, environ);
    }
#if HPCJAVA_FORK_OPJITCONV
  } 
  else if (childpid == -1) {
    fprintf(stderr, "Failed to exec %s: %s\n",
	    jitconv_pgm, strerror(errno));
  } 
#endif
}


/***
 * The VM death event notifies the agent of the termination of the VM. 
 * No events will occur after the VMDeath event.
 * In the case of VM start-up failure, this event will not be sent. 
 * Note that Agent_OnUnload will still be called in these cases.
 ***/
static void JNICALL
cb_vm_death(jvmtiEnv *jvmti_env,
            JNIEnv* jni_env)
{
  /************* hpcrun critical section ************/
  if (!hpcrun_safe_enter()) {
    return ;
  }

  if (debug)
    fprintf(stderr, "vm_death\n");

  jmt_get_all_methods_db(jvmti);
	
  /************* end hpcrun critical section ************/
  hpcrun_safe_exit();
}


/*****
 * setting up for the output file (which is a *.jo file)
 * return true if everything works fine, false otherwise
 ****/
static bool
setup_file_output()
{
  /* record time start (to be used for opjitconv) */
  gettimeofday(&time_start, NULL);

  /* prepare the name of the .dump and  .jo file */
  int rank = hpcrun_get_rank();
  rank = (rank<0? 0:rank);
  thread_data_t* td = hpcrun_get_thread_data();

  // temporary solution : we need to find a uniform way to dump file
  int thread_id = (td==NULL? 0 : (td->core_profile_trace_data.id) );
  int ret = snprintf(file_dump,PATH_MAX,"%s/java-%06d-%03d.dump",hpcrun_get_directory(), rank, thread_id);

  if (ret >= PATH_MAX) {
    perror("java dump filename is too long\n");
    return false;
  }

  if (debug)
    fprintf(stderr,"Java file to dump: %s\n", file_dump);

  /* initialize Java's tree interval */
  char file_dump_jo[PATH_MAX] = {'\0'};
  snprintf(file_dump_jo,PATH_MAX,"%s.jo",file_dump);
  hpcjava_interval_tree_init(file_dump_jo);

  /* prepare java dump file */
  agent_hdl = op_open_agent(file_dump);
  if (!agent_hdl) {
    perror("Error: op_open_agent()");
    return false;
  }
  return true;
}


/*****
 * setting up for java capabilities
 * return true if everything works fine, false otherwise
 ****/
static bool
setup_capabilities(jvmtiEnv * jvmti)
{
  jvmtiCapabilities caps;
  jvmtiError error;
  jvmtiJlocationFormat format;

  can_get_line_numbers = 0;

  /* FIXME: settable through command line, default on/off? */
  error = (*jvmti)->GetJLocationFormat(jvmti, &format);
  if (!handle_error(error, "GetJLocationFormat", 1) &&
      format == JVMTI_JLOCATION_JVMBCI) 
  {
    memset(&caps, '\0', sizeof(caps));

    caps.can_generate_compiled_method_load_events = 1;
    caps.can_get_line_numbers 		= 1;
    caps.can_get_source_file_name 	= 1;
    caps.can_get_bytecodes 		= 1;
    caps.can_get_constant_pool 		= 1;

    error = (*jvmti)->AddCapabilities(jvmti, &caps);
    if (!handle_error(error, "AddCapabilities()", 1))
      can_get_line_numbers = 1;
  }
  return (can_get_line_numbers == 1);
}


/******
 * setup jvmti callbacks
 * return true if the callbacks are succesfully registered,
 *	false otherwise
 *****/
static bool
setup_callbacks(jvmtiEnv * jvmti)
{
  jvmtiEventCallbacks callbacks;
  jvmtiError error;

  memset(&callbacks, 0, sizeof(callbacks));

  callbacks.ClassLoad			= cb_class_load;
  callbacks.ClassPrepare		= cb_class_prepare;
  callbacks.CompiledMethodLoad 		= cb_compiled_method_load;
  callbacks.CompiledMethodUnload 	= cb_compiled_method_unload;
  callbacks.DynamicCodeGenerated 	= cb_dynamic_code_generated;
  callbacks.VMDeath			= cb_vm_death;

  error = (*jvmti)->SetEventCallbacks(jvmti, &callbacks,
				      sizeof(callbacks));
  if (handle_error(error, "SetEventCallbacks()", 1)) {
     return false;
  }

  /*
   *  register event callbacks
   */

  JAVA_REGISTER_EVENT( JVMTI_EVENT_COMPILED_METHOD_LOAD )
  JAVA_REGISTER_EVENT( JVMTI_EVENT_COMPILED_METHOD_UNLOAD )
  JAVA_REGISTER_EVENT( JVMTI_EVENT_DYNAMIC_CODE_GENERATED )
  JAVA_REGISTER_EVENT( JVMTI_EVENT_CLASS_PREPARE )
  JAVA_REGISTER_EVENT( JVMTI_EVENT_CLASS_LOAD )
  JAVA_REGISTER_EVENT( JVMTI_EVENT_VM_DEATH )

  return true;
}


static ASGCTType
get_asgct()
{
   void *handle = dlopen("libjvm.so", RTLD_LAZY);

   if (handle != NULL) {
      return (ASGCTType) dlsym(handle, "AsyncGetCallTrace");
   }
   return NULL;
}
  

/***
 * The VM will start the agent by calling this function. It will be called early enough in VM initialization that:
 *
 *     system properties may be set before they have been used in the start-up of the VM
 *     the full set of capabilities is still available (note that capabilities that 
 *     		configure the VM may only be available at this time
 *     			--see the Capability function section)
 *     no bytecodes have executed
 *     no classes have been loaded
 *     no objects have been created
 *
 */
JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM * jvm, char * options, void * reserved)
{
  jint rc;
  jint res = 0;
	
  /************* hpcrun critical section ************/
  if (!hpcrun_safe_enter()) {
    return 0;
  }

  java_vm = jvm;

  debug = (options && !strcmp("debug", options));

  if (debug)
    fprintf(stderr, "jvmti hpcjava: agent activated\n");

  /* shut up compiler warning */
  reserved = reserved;

  /* initialize output file */
  if (!setup_file_output()) {
    res = -1;
    goto finalize;
  }

  /* get jvmti from jvm */
  rc = (*jvm)->GetEnv(jvm, (void *)&jvmti, JVMTI_VERSION_1);
  if (rc != JNI_OK) {
    fprintf(stderr, "Error: GetEnv(), rc=%i\n", rc);
    res = -1;
    goto finalize;
  }

  /* get Java's undocumented AsyncGetCallTrace method */
  ASGCTType asgct = get_asgct();
  js_setAsgct(asgct);

  /** inform hpcjava to store references for jvm and jvmti 
   ** these references can be used to find Java call stack 
   **/
  hpcjava_set_jvmti(jvm, jvmti);

  /* setup jvmti capabilities */
  if (!setup_capabilities(jvmti)) {
    res = -1;
    goto finalize;
  }

  /*
   *  set function callbacks
   */
 if (!setup_callbacks(jvmti)) {
    res = -1;
    goto finalize;
 }

 finalize:
  /************* end hpcrun critical section ************/
  hpcrun_safe_exit();

  return res;
}


/*****
 * Agent_OnUnload: This is called immediately before the shared library is unloaded
 *
 * This function will be called by the VM when the library is about to be unloaded. 
 * The library will be unloaded and this function will be called if 
 *  some platform specific mechanism causes the unload 
 *  (an unload mechanism is not specified in this document) or the library is (in effect) 
 *  unloaded by the termination of the VM whether through normal termination or VM failure, 
 *  including start-up failure. Uncontrolled shutdown is, of couse, an exception to this rule. 
 */
JNIEXPORT void JNICALL Agent_OnUnload(JavaVM * jvm)
{
  /************* hpcrun critical section ************/
  if (!hpcrun_safe_enter()) {
    return ;
  }

  /* shut up compiler warning */
  jvm = jvm;

  hpcjava_delete_ui();

  if (op_close_agent(agent_hdl))
    perror("Error: op_close_agent()");

  libhpcjava_fini();

  if (debug)
    fprintf(stderr, "jvm unload\n");

  /************* end hpcrun critical section ************/
  hpcrun_safe_exit();
}
