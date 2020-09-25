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

#ifndef PTI_SAMPLES_UTILS_ZE_TRACER_H_
#define PTI_SAMPLES_UTILS_ZE_TRACER_H_

#include <assert.h>

#include <set>

#include <L0/tracing_api.h>

#define ZE_FUNCTION_COUNT      (ze_tracing::ZE_FUNCTION_COUNT)
#define ZE_CALLBACK_SITE_ENTER (ze_tracing::ZE_CALLBACK_SITE_ENTER)
#define ZE_CALLBACK_SITE_EXIT  (ze_tracing::ZE_CALLBACK_SITE_EXIT)

using callback_data_t = ze_tracing::callback_data_t;
using function_id_t = ze_tracing::function_id_t;
using tracing_callback_t = ze_tracing::tracing_callback_t;

class ZeTracer {
 public:
  ZeTracer(ze_driver_handle_t driver,
           tracing_callback_t callback,
           void* user_data) {
    assert(driver != nullptr);

    data_.callback = callback;
    data_.user_data = user_data;

    ze_result_t status = ZE_RESULT_SUCCESS;
    zet_tracer_desc_t tracer_desc = {};
    tracer_desc.version = ZET_TRACER_DESC_VERSION_CURRENT;
    tracer_desc.pUserData = &data_;

    status = zetTracerCreate(driver, &tracer_desc, &handle_);
    assert(status == ZE_RESULT_SUCCESS);
  }

  ~ZeTracer() {
    if (handle_ != nullptr) {
      ze_result_t status = ZE_RESULT_SUCCESS;
      status = zetTracerDestroy(handle_);
      assert(status == ZE_RESULT_SUCCESS);
    }
  }

  bool SetTracingFunction(function_id_t function) {
    if (!IsValid()) {
      return false;
    }

    if (function >= 0 && function < ZE_FUNCTION_COUNT) {
      functions_.insert(function);
      return true;
    }

    return false;
  }

  bool Enable() {
    if (!IsValid()) {
      return false;
    }

    ze_tracing::SetTracingFunctions(handle_, functions_);

    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zetTracerSetEnabled(handle_, true);
    if (status != ZE_RESULT_SUCCESS) {
      return false;
    }

    return true;
  }

  bool Disable() {
    if (!IsValid()) {
      return false;
    }

    ze_result_t status = ZE_RESULT_SUCCESS;
    status = zetTracerSetEnabled(handle_, false);
    if (status != ZE_RESULT_SUCCESS) {
      return false;
    }

    return true;
  }

  bool IsValid() const {
    return (handle_ != nullptr);
  }

 private:
  zet_tracer_handle_t handle_ = nullptr;
  std::set<function_id_t> functions_;
  ze_tracing::global_data_t data_;
};

#endif // PTI_SAMPLES_UTILS_ZE_TRACER_H_
