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
#include <string.h>

#include <string>
#include <vector>

#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

namespace utils {
namespace ze {

inline void GetIntelDeviceAndDriver(ze_device_type_t type,
                                    ze_device_handle_t& device,
                                    ze_driver_handle_t& driver) {
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t driver_count = 0;
  status = zeDriverGet(&driver_count, nullptr);
  if (status != ZE_RESULT_SUCCESS || driver_count == 0) {
    return;
  }

  std::vector<ze_driver_handle_t> driver_list(driver_count, nullptr);
  status = zeDriverGet(&driver_count, driver_list.data());
  assert(status == ZE_RESULT_SUCCESS);

  for (uint32_t i = 0; i < driver_count; ++i) {
    uint32_t device_count = 0;
    status = zeDeviceGet(driver_list[i], &device_count, nullptr);
    if (status != ZE_RESULT_SUCCESS || device_count == 0) {
        continue;
    }

    std::vector<ze_device_handle_t> device_list(device_count, nullptr);
    status = zeDeviceGet(driver_list[i], &device_count, device_list.data());
    assert(status == ZE_RESULT_SUCCESS);

    for (uint32_t j = 0; j < device_count; ++j) {
      ze_device_properties_t props;
      props.version = ZE_DEVICE_PROPERTIES_VERSION_CURRENT;
      status = zeDeviceGetProperties(device_list[j], &props);
      assert(status == ZE_RESULT_SUCCESS);

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
  assert(status == ZE_RESULT_SUCCESS);
  return props.name;
}

static int GetMetricId(zet_metric_group_handle_t group, std::string name) {
  assert(group != nullptr);

  ze_result_t status = ZE_RESULT_SUCCESS;
  uint32_t metric_count = 0;
  status = zetMetricGet(group, &metric_count, nullptr);
  assert(status == ZE_RESULT_SUCCESS);

  if (metric_count == 0) {
    return -1;
  }

  std::vector<zet_metric_handle_t> metric_list(metric_count, nullptr);
  status = zetMetricGet(group, &metric_count, metric_list.data());
  assert(status == ZE_RESULT_SUCCESS);

  int target = -1;
  for (uint32_t i = 0; i < metric_count; ++i) {
    zet_metric_properties_t metric_props = {};
    metric_props.version = ZET_METRIC_PROPERTIES_VERSION_CURRENT;
    status = zetMetricGetProperties(metric_list[i], &metric_props);
    assert(status == ZE_RESULT_SUCCESS);

    if (name == metric_props.name) {
      target = i;
      break;
    }
  }

  return target;
}

static zet_metric_group_handle_t FindMetricGroup(
    ze_device_handle_t device, std::string name,
    zet_metric_group_sampling_type_t type) {
  assert(device != nullptr);
  
  ze_result_t status = ZE_RESULT_SUCCESS;
  uint32_t group_count = 0;
  status = zetMetricGroupGet(device, &group_count, nullptr);
  assert(status == ZE_RESULT_SUCCESS);
  if (group_count == 0) {
    return nullptr;
  }

  std::vector<zet_metric_group_handle_t> group_list(group_count, nullptr);
  status = zetMetricGroupGet(device, &group_count, group_list.data());
  assert(status == ZE_RESULT_SUCCESS);

  zet_metric_group_handle_t target = nullptr;
  for (uint32_t i = 0; i < group_count; ++i) {
    zet_metric_group_properties_t group_props = {};
    group_props.version = ZET_METRIC_GROUP_PROPERTIES_VERSION_CURRENT;
    status = zetMetricGroupGetProperties(group_list[i], &group_props);
    assert(status == ZE_RESULT_SUCCESS);

    if (name == group_props.name && type == group_props.samplingType) {
      target = group_list[i];
      break;
    }
  }

  return target;
}

} // namespace ze
} // namespace utils

#endif // PTI_SAMPLES_UTILS_ZE_UTILS_H_
