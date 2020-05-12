#ifndef Analysis_Advisor_GPUInstruction_hpp 
#define Analysis_Advisor_GPUInstruction_hpp

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

#include <lib/analysis/MetricNameProfMap.hpp>

#include "GPUOptimizer.hpp"
#include "GPUAdvisor.hpp"


namespace Analysis {

namespace CallPath {

void
overlayGPUInstructionsMain(const std::vector<std::string> &instruction_files,
  Prof::CallPath::Profile &prof, TotalBlames &total_blames, GPUAdvisor &gpu_advisor);

}

}

#endif  // Analysis_Advisor_GPUInstruction_hpp
