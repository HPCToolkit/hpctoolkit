#ifndef Analysis_Advisor_GPUInstruction_hpp
#define Analysis_Advisor_GPUInstruction_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <stack>
#include <string>
#include <vector>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/binutils/LM.hpp>
#include <lib/prof/CallPath-Profile.hpp>
#include <lib/prof/Struct-Tree.hpp>

#include "GPUAdvisor.hpp"

namespace Analysis {

namespace CallPath {

std::vector<GPUAdvisor::AdviceTuple> overlayGPUInstructionsMain(
    Prof::CallPath::Profile &prof, const std::vector<std::string> &instruction_files,
    const std::string &gpu_arch);

}

}  // namespace Analysis

#endif  // Analysis_Advisor_GPUInstruction_hpp
