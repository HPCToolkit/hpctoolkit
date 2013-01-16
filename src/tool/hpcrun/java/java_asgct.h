#ifndef _JAVA_ASGCT_H
#define _JAVA_ASGCT_H

#include <jvmti.h>

// call frame copied from old .h file and renamed
typedef struct {
    jint lineno;                      // line number in the source file
    jmethodID method_id;              // method executed in this frame
} ASGCT_CallFrame;

// call trace copied from old .h file and renamed
typedef struct {
    JNIEnv *env_id;                   // Env where trace was recorded
    jint num_frames;                  // number of frames in this trace
    ASGCT_CallFrame *frames;          // frames
} ASGCT_CallTrace;

void AsyncGetCallTrace(ASGCT_CallTrace *trace, jint depth, void* ucontext);

#endif // _JAVA_ASGCT_H
