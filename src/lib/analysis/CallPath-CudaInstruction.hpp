#ifndef Analysis_CallPath_CallPath_CudaInstruction_hpp 
#define Analysis_CallPath_CallPath_CudaInstruction_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <vector>
#include <stack>
#include <string>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/binutils/LM.hpp>

#include <lib/prof/CallPath-Profile.hpp>
#include <lib/prof/Struct-Tree.hpp>


namespace Analysis {

namespace CallPath {

void
overlayCudaInstructionsMain(Prof::CallPath::Profile &prof,
  const std::vector<std::string> &instruction_files);

}

}

#endif  // Analysis_CallPath_CallPath_CudaInstruction_hpp
