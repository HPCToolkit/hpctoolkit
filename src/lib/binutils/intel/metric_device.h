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
// Copyright ((c)) 2002-2022, Rice University
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

//==============================================================
// Copyright © 2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// =============================================================

#ifndef PTI_SAMPLES_UTILS_METRIC_DEVICE_H_
#define PTI_SAMPLES_UTILS_METRIC_DEVICE_H_

#include "metric_utils.h"
#include "shared_library.h"

namespace md = MetricsDiscovery;

class MetricDevice {
public:
  static MetricDevice* Create() {
    SharedLibrary* lib = nullptr;

    for (auto& path : utils::metrics::GetMDLibraryPossiblePaths()) {
      lib = SharedLibrary::Create(path);
      if (lib != nullptr) {
        break;
      }
    }

    if (lib != nullptr) {
      md::IMetricsDevice_1_5* device = nullptr;

      md::OpenMetricsDevice_fn OpenMetricsDevice =
          lib->GetSym<md::OpenMetricsDevice_fn>("OpenMetricsDevice");
      md::TCompletionCode status = OpenMetricsDevice(&device);
      if (status != md::CC_OK && status != md::CC_ALREADY_INITIALIZED)
        std::abort();

      if (device != nullptr) {
        return new MetricDevice(device, lib);
      } else {
        delete lib;
      }
    }

    return nullptr;
  }

  ~MetricDevice() {
    assert(device_ != nullptr);
    md::CloseMetricsDevice_fn CloseMetricsDevice =
        lib_->GetSym<md::CloseMetricsDevice_fn>("CloseMetricsDevice");
    md::TCompletionCode status = CloseMetricsDevice(device_);
    if (status != md::CC_OK)
      std::abort();

    assert(lib_ != nullptr);
    delete lib_;
  }

  md::IMetricsDevice_1_5* operator->() const { return device_; }

private:
  MetricDevice(md::IMetricsDevice_1_5* device, SharedLibrary* lib) : device_(device), lib_(lib) {}

  md::IMetricsDevice_1_5* device_ = nullptr;
  SharedLibrary* lib_ = nullptr;
};

#endif  // PTI_SAMPLES_UTILS_METRIC_DEVICE_H_
