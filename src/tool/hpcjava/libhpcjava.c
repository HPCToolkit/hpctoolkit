#include <stdio.h>
#include <jvmti.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/time.h>

#include <ui_tree_java.h>
//#include <hpcrun/unwind/common/ui_tree_java.h>

typedef void fct_add_interval(void*,void*);

static int debug = 0;
static int can_get_line_numbers = 1;

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
 * returned array is map_length length, params map and map_length != 0
 * format of lineno information is JVMTI_JLOCATION_JVMBCI, map is an array
 * of { address, code byte index }, table_ptr an array of { byte code index,
 * lineno }
 */
struct debug_line_info {
        unsigned long vma;
        unsigned int lineno;
        /* The filename format is unspecified, absolute path, relative etc. */
        char const * filename;
};

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

	if (debug) {
		for (i = 0; i < map_length; ++i) {
			fprintf(stderr, "%d\t-> %lx %s:%d\t\n", i, debug_line[i].vma,
				debug_line[i].filename,debug_line[i].lineno);
		}
		fprintf(stderr, "\n");
		fflush(stderr);
	}
	
	return debug_line;
}


static void JNICALL cb_compiled_method_load(jvmtiEnv * jvmti,
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
			/*
			   int i;
			   for (i=0; i<map_length; i++) {
			   	const void *seg_start = map[i].start_address;
				const void *seg_end;
				if (i<map_length-1) {
				   seg_end = map[i+1].start_address;
				} else {
				   seg_end = code_addr + code_size;
				}
				hpcjava_add_address_interval(seg_start, seg_end);
			   }	
			*/		   	
			/*	debug_line =
					create_debug_line_info(map_length, map,
						entry_count, table_ptr,
						source_filename);
			 */
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
	}

cleanup:
	(*jvmti)->Deallocate(jvmti, (unsigned char *)method_name);
	(*jvmti)->Deallocate(jvmti, (unsigned char *)method_signature);
cleanup1:
	(*jvmti)->Deallocate(jvmti, (unsigned char *)class_signature);
	(*jvmti)->Deallocate(jvmti, (unsigned char *)table_ptr);
	(*jvmti)->Deallocate(jvmti, (unsigned char *)source_filename);
cleanup2:
	free(debug_line);
}


static void JNICALL cb_compiled_method_unload(jvmtiEnv * jvmti_env,
	jmethodID method, void const * code_addr)
{
	/* shut up compiler warning */
	jvmti_env = jvmti_env;
	method = method;

	if (debug)
		fprintf(stderr, "unload: addr=%p\n", code_addr);
}


static void JNICALL cb_dynamic_code_generated(jvmtiEnv * jvmti_env,
	char const * name, void const * code_addr, jint code_size)
{
	/* shut up compiler warning */
	jvmti_env = jvmti_env;
	if (debug) {
		fprintf(stderr, "dyncode: name=%s, addr=%p, size=%i \n",
			name, code_addr, code_size); 
	}
}


JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM * jvm, char * options, void * reserved)
{
	jint rc;
	jvmtiEnv * jvmti = NULL;
	jvmtiEventCallbacks callbacks;
	jvmtiCapabilities caps;
	jvmtiJlocationFormat format;
	jvmtiError error;

	/* shut up compiler warning */
	reserved = reserved;

	hpcjava_interval_tree_init();

	if (options && !strcmp("version", options)) {
		return -1;
	}

	if (options && !strcmp("debug", options))
		debug = 1;

	if (debug)
		fprintf(stderr, "jvmti hpcjava: agent activated\n");

	rc = (*jvm)->GetEnv(jvm, (void *)&jvmti, JVMTI_VERSION_1);
	if (rc != JNI_OK) {
		fprintf(stderr, "Error: GetEnv(), rc=%i\n", rc);
		return -1;
	}

	memset(&caps, '\0', sizeof(caps));
	caps.can_generate_compiled_method_load_events = 1;
	error = (*jvmti)->AddCapabilities(jvmti, &caps);
	if (handle_error(error, "AddCapabilities()", 1))
		return -1;

	/* FIXME: settable through command line, default on/off? */
	error = (*jvmti)->GetJLocationFormat(jvmti, &format);
	if (!handle_error(error, "GetJLocationFormat", 1) &&
	    format == JVMTI_JLOCATION_JVMBCI) {
		memset(&caps, '\0', sizeof(caps));
		caps.can_get_line_numbers = 1;
		caps.can_get_source_file_name = 1;
		error = (*jvmti)->AddCapabilities(jvmti, &caps);
		if (!handle_error(error, "AddCapabilities()", 1))
			can_get_line_numbers = 1;
	}

	memset(&callbacks, 0, sizeof(callbacks));
	callbacks.CompiledMethodLoad = cb_compiled_method_load;
	callbacks.CompiledMethodUnload = cb_compiled_method_unload;
	callbacks.DynamicCodeGenerated = cb_dynamic_code_generated;
	error = (*jvmti)->SetEventCallbacks(jvmti, &callbacks,
					    sizeof(callbacks));
	if (handle_error(error, "SetEventCallbacks()", 1))
		return -1;

	error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
			JVMTI_EVENT_COMPILED_METHOD_LOAD, NULL);
	if (handle_error(error, "SetEventNotificationMode() "
			 "JVMTI_EVENT_COMPILED_METHOD_LOAD", 1))
		return -1;
	error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
			JVMTI_EVENT_COMPILED_METHOD_UNLOAD, NULL);
	if (handle_error(error, "SetEventNotificationMode() "
			 "JVMTI_EVENT_COMPILED_METHOD_UNLOAD", 1))
		return -1;
	error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
			JVMTI_EVENT_DYNAMIC_CODE_GENERATED, NULL);
	if (handle_error(error, "SetEventNotificationMode() "
			 "JVMTI_EVENT_DYNAMIC_CODE_GENERATED", 1))
		return -1;
	return 0;
}


JNIEXPORT void JNICALL Agent_OnUnload(JavaVM * jvm)
{
	/* shut up compiler warning */
	jvm = jvm;

	hpcjava_delete_ui();
}
