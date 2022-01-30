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

#ifndef PTI_SAMPLES_UTILS_ZE_UTILS_H_
#define PTI_SAMPLES_UTILS_ZE_UTILS_H_

#include <assert.h>
#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>
#include <string.h>
#include <string>
#include <vector>

namespace utils {
namespace ze {

inline void GetIntelDeviceAndDriver(
    ze_device_type_t type, ze_device_handle_t& device, ze_driver_handle_t& driver) {
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t driver_count = 0;
  status = zeDriverGet(&driver_count, nullptr);
  if (status != ZE_RESULT_SUCCESS || driver_count == 0) {
    return;
  }

  std::vector<ze_driver_handle_t> driver_list(driver_count, nullptr);
  status = zeDriverGet(&driver_count, driver_list.data());
  if (status != ZE_RESULT_SUCCESS)
    std::abort();

  for (uint32_t i = 0; i < driver_count; ++i) {
    uint32_t device_count = 0;
    status = zeDeviceGet(driver_list[i], &device_count, nullptr);
    if (status != ZE_RESULT_SUCCESS || device_count == 0) {
      continue;
    }

    std::vector<ze_device_handle_t> device_list(device_count, nullptr);
    status = zeDeviceGet(driver_list[i], &device_count, device_list.data());
    if (status != ZE_RESULT_SUCCESS)
      std::abort();

    for (uint32_t j = 0; j < device_count; ++j) {
      ze_device_properties_t props;
      props.version = ZE_DEVICE_PROPERTIES_VERSION_CURRENT;
      status = zeDeviceGetProperties(device_list[j], &props);
      if (status != ZE_RESULT_SUCCESS)
        std::abort();

      if (props.type == type && strstr(props.name, "Intel") != nullptr) {
        device = device_list[j];
        driver = driver_list[i];
        break;
      }
    }
  }

  return;
}

inline std::string GetDeviceName(ze_device_handle_t device) {
  assert(device != nullptr);
  ze_result_t status = ZE_RESULT_SUCCESS;
  ze_device_properties_t props;
  props.version = ZE_DEVICE_PROPERTIES_VERSION_CURRENT;
  status = zeDeviceGetProperties(device, &props);
  if (status != ZE_RESULT_SUCCESS)
    std::abort();
  return props.name;
}

static int GetMetricId(zet_metric_group_handle_t group, std::string name) {
  assert(group != nullptr);

  ze_result_t status = ZE_RESULT_SUCCESS;
  uint32_t metric_count = 0;
  status = zetMetricGet(group, &metric_count, nullptr);
  if (status != ZE_RESULT_SUCCESS)
    std::abort();

  if (metric_count == 0) {
    return -1;
  }

  std::vector<zet_metric_handle_t> metric_list(metric_count, nullptr);
  status = zetMetricGet(group, &metric_count, metric_list.data());
  if (status != ZE_RESULT_SUCCESS)
    std::abort();

  int target = -1;
  for (uint32_t i = 0; i < metric_count; ++i) {
    zet_metric_properties_t metric_props = {};
    metric_props.version = ZET_METRIC_PROPERTIES_VERSION_CURRENT;
    status = zetMetricGetProperties(metric_list[i], &metric_props);
    if (status != ZE_RESULT_SUCCESS)
      std::abort();

    if (name == metric_props.name) {
      target = i;
      break;
    }
  }

  return target;
}

static zet_metric_group_handle_t FindMetricGroup(
    ze_device_handle_t device, std::string name, zet_metric_group_sampling_type_t type) {
  assert(device != nullptr);

  ze_result_t status = ZE_RESULT_SUCCESS;
  uint32_t group_count = 0;
  status = zetMetricGroupGet(device, &group_count, nullptr);
  if (status != ZE_RESULT_SUCCESS)
    std::abort();
  if (group_count == 0) {
    return nullptr;
  }

  std::vector<zet_metric_group_handle_t> group_list(group_count, nullptr);
  status = zetMetricGroupGet(device, &group_count, group_list.data());
  if (status != ZE_RESULT_SUCCESS)
    std::abort();

  zet_metric_group_handle_t target = nullptr;
  for (uint32_t i = 0; i < group_count; ++i) {
    zet_metric_group_properties_t group_props = {};
    group_props.version = ZET_METRIC_GROUP_PROPERTIES_VERSION_CURRENT;
    status = zetMetricGroupGetProperties(group_list[i], &group_props);
    if (status != ZE_RESULT_SUCCESS)
      std::abort();

    if (name == group_props.name && type == group_props.samplingType) {
      target = group_list[i];
      break;
    }
  }

  return target;
}
}  // namespace ze
}  // namespace utils

#endif  // PTI_SAMPLES_UTILS_ZE_UTILS_H_
