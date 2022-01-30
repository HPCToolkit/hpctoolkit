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

#ifndef PTI_SAMPLES_UTILS_ZE_TRACER_H_
#define PTI_SAMPLES_UTILS_ZE_TRACER_H_

#include <assert.h>
#include <L0/tracing_api.h>
#include <set>

#define ZE_FUNCTION_COUNT      (ze_tracing::ZE_FUNCTION_COUNT)
#define ZE_CALLBACK_SITE_ENTER (ze_tracing::ZE_CALLBACK_SITE_ENTER)
#define ZE_CALLBACK_SITE_EXIT  (ze_tracing::ZE_CALLBACK_SITE_EXIT)

using callback_data_t = ze_tracing::callback_data_t;
using function_id_t = ze_tracing::function_id_t;
using tracing_callback_t = ze_tracing::tracing_callback_t;

class ZeTracer {
public:
  ZeTracer(ze_driver_handle_t driver, tracing_callback_t callback, void* user_data) {
    assert(driver != nullptr);

    data_.callback = callback;
    data_.user_data = user_data;

    ze_result_t status = ZE_RESULT_SUCCESS;
    zet_tracer_desc_t tracer_desc = {};
    tracer_desc.version = ZET_TRACER_DESC_VERSION_CURRENT;
    tracer_desc.pUserData = &data_;

    status = zetTracerCreate(driver, &tracer_desc, &handle_);
    if (status != ZE_RESULT_SUCCESS)
      abort();
  }

  ~ZeTracer() {
    if (handle_ != nullptr) {
      ze_result_t status = ZE_RESULT_SUCCESS;
      status = zetTracerDestroy(handle_);
      if (status != ZE_RESULT_SUCCESS)
        abort();
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

  bool IsValid() const { return (handle_ != nullptr); }

private:
  zet_tracer_handle_t handle_ = nullptr;
  std::set<function_id_t> functions_;
  ze_tracing::global_data_t data_;
};

#endif  // PTI_SAMPLES_UTILS_ZE_TRACER_H_
