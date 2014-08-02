//
// Get hardware thread id
//
// !!! WARNING !!!
//
// As of 29 jul 2014, this simple function CANNOT be a 'static inline' in a .h file
// This function MUST BE compiled with a backend compiler. Using the #include mechanism
// would use the front end compiler.
// 
// When this changes, remove this warning
//
// !!!! END WARNING !!!!!
//

#include "hardware-thread-id.h"
#include <spi/include/kernel/location.h>

int
get_hw_tid(void)
{
  return Kernel_ProcessorID();
}
