// SPDX-FileCopyrightText: 2022-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef blame_shift_opencl_h
#define blame_shift_opencl_h

//******************************************************************************
// local includes
//******************************************************************************

#include <CL/cl.h>
#include "../../../foil/opencl.h"


//******************************************************************************
// interface operations
//******************************************************************************

void
opencl_queue_prologue
(
 cl_command_queue queue
);


void
opencl_queue_epilogue
(
 cl_command_queue queue
);


void
opencl_kernel_prologue
(
  cl_event event,
  uint32_t kernel_module_id,
  const struct hpcrun_foil_appdispatch_opencl* dispatch
);


void
opencl_kernel_epilogue
(
  cl_event event,
  const struct hpcrun_foil_appdispatch_opencl* dispatch
);


void
opencl_sync_prologue
(
 cl_command_queue queue
);


void
opencl_sync_epilogue
(
  cl_command_queue queue,
  uint16_t num_sync_events,
  const struct hpcrun_foil_appdispatch_opencl* dispatch
);

#endif  //blame_shift_opencl_h
