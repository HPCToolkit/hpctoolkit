//==============================================================
// Copyright © 2019 Intel Corporation
//
// SPDX-License-Identifier: MIT
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