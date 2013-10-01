#include <assert.h>
#include <dlfcn.h>
#include <jvmti.h>
#include <jni.h>

#ifndef STACKTRACES_H
#define STACKTRACES_H

// To implement the profiler, we rely on an undocumented function called
// AsyncGetCallTrace in the Java virtual machine, which is used by Sun
// Studio Analyzer, and designed to get stack traces asynchronously.
// It uses the old JVMPI interface, so we must reconstruct the
// neccesary bits of that here.

// For a Java frame, the lineno is the bci of the method, and the
// method_id is the jmethodID.  For a JNI method, the lineno is -3,
// and the method_id is the jmethodID.
typedef struct {
  jint lineno;
  jmethodID method_id;
} JVMPI_CallFrame;

typedef struct {
  // JNIEnv of the thread from which we grabbed the trace
  JNIEnv *env_id;
  // < 0 if the frame isn't walkable
  jint num_frames;
  // The frames, callee first.
  JVMPI_CallFrame *frames;
} JVMPI_CallTrace;

typedef void (*ASGCTType)(JVMPI_CallTrace *, jint, void *);

void js_setAsgct(ASGCTType asgct_func);


#endif  // STACKTRACES_H

