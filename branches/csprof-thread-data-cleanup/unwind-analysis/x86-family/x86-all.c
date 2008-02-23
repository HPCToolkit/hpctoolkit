/* the xed include files used in the x86 family of functions define a lot of static functions. 
 * to avoid replicated copies, we should include xed only once. since each of the x86 family 
 * of files includes xed, we do the next best thing - we compile all of our xed-based source 
 * files into a single object file.
 */

#include "x86-addsub.c"
#include "x86-build-intervals.c"
#include "x86-call.c"
#include "x86-canonical.c"
#include "x86-debug.c"
#include "x86-decoder.c"
#include "x86-enter.c"
#include "x86-jump.c"
#include "x86-leave.c"
#include "x86-move.c"
#include "x86-process-inst.c"
#include "x86-push.c"
#include "x86-return.c"
