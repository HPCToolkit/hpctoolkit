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
        assert(status == md::CC_OK ||
               status == md::CC_ALREADY_INITIALIZED);

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
    assert(status == md::CC_OK);

    assert(lib_ != nullptr);
    delete lib_;
  }

  md::IMetricsDevice_1_5* operator->() const {
    return device_;
  }

private:
  MetricDevice(md::IMetricsDevice_1_5* device, SharedLibrary* lib)
      : device_(device), lib_(lib) {}

  md::IMetricsDevice_1_5* device_ = nullptr;
  SharedLibrary* lib_ = nullptr;
};

#endif // PTI_SAMPLES_UTILS_METRIC_DEVICE_H_
