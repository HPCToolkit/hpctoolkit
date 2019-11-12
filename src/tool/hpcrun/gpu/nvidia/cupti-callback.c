
void
cupti_subscriber_callback
(
 void *userdata,
 CUpti_CallbackDomain domain,
 CUpti_CallbackId cb_id,
 const void *cb_info
)
{
  if (domain == CUPTI_CB_DOMAIN_RESOURCE) {
    const CUpti_ResourceData *rd = (const CUpti_ResourceData *) cb_info;
    if (cb_id == CUPTI_CBID_RESOURCE_MODULE_LOADED) {
      CUpti_ModuleResourceData *mrd = (CUpti_ModuleResourceData *)
        rd->resourceDescriptor;

      PRINT("loaded module id %d, cubin size %" PRIu64 ", cubin %p\n", 
        mrd->moduleId, mrd->cubinSize, mrd->pCubin);
      DISPATCH_CALLBACK(cupti_load_callback, (mrd->moduleId, mrd->pCubin, mrd->cubinSize));
    } else if (cb_id == CUPTI_CBID_RESOURCE_MODULE_UNLOAD_STARTING) {
      CUpti_ModuleResourceData *mrd = (CUpti_ModuleResourceData *)
        rd->resourceDescriptor;

      PRINT("unloaded module id %d, cubin size %" PRIu64 ", cubin %p\n",
        mrd->moduleId, mrd->cubinSize, mrd->pCubin);
      DISPATCH_CALLBACK(cupti_unload_callback, (mrd->moduleId, mrd->pCubin, mrd->cubinSize));
    } else if (cb_id == CUPTI_CBID_RESOURCE_CONTEXT_CREATED) {
      cupti_enable_activities(rd->context);
    }
  } else if (domain == CUPTI_CB_DOMAIN_DRIVER_API) {
    // stop flag is only set if a driver or runtime api called
    cupti_stop_flag_set();

#if 0
    // johnmc: replaced with lazy initialization
    cupti_correlation_channel_init();
    cupti_activity_channel_init();
#endif

    const CUpti_CallbackData *cd = (const CUpti_CallbackData *) cb_info;

    bool ompt_runtime_api_flag = ompt_runtime_status_get();
#if 0
    bool is_valid_op = false;
    bool is_copy_op = false;
    bool is_copyin_op = false;
    bool is_copyout_op = false;
    bool is_alloc_op = false;
    bool is_delete_op = false;
    bool is_sync_op = false;
    bool is_kernel_op = false;
#endif
    gpu_op_placeholder_flags_t gpu_op_placeholder_flags;
    ip_normalized_t func_ip;
    switch (cb_id) {
      //FIXME(Keren): do not support memory allocate and free for current CUPTI version
      //case CUPTI_DRIVER_TRACE_CBID_cuMemAlloc:
      //case CUPTI_DRIVER_TRACE_CBID_cu64MemAlloc:
      //case CUPTI_DRIVER_TRACE_CBID_cuMemAllocPitch:
      //case CUPTI_DRIVER_TRACE_CBID_cu64MemAllocPitch:
      //case CUPTI_DRIVER_TRACE_CBID_cuMemAlloc_v2:
      //case CUPTI_DRIVER_TRACE_CBID_cuMemAllocPitch_v2:
      //  {
      //    cuda_state = cuda_placeholders.cuda_memalloc_state;
      //    is_valid_op = true;
      //    break;
      //  }
      // synchronize apis
      case CUPTI_DRIVER_TRACE_CBID_cuCtxSynchronize:
      case CUPTI_DRIVER_TRACE_CBID_cuEventSynchronize:
      case CUPTI_DRIVER_TRACE_CBID_cuStreamSynchronize:
      case CUPTI_DRIVER_TRACE_CBID_cuStreamSynchronize_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuStreamWaitEvent:
      case CUPTI_DRIVER_TRACE_CBID_cuStreamWaitEvent_ptsz:
        {
	  gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, 
				       gpu_placeholder_type_sync);
          is_valid_op = true;
          break;
        }
      // copyin apis
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoD:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoDAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoD_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoDAsync_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoD_v2_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoDAsync_v2_ptsz:
        {
	  gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, 
				       gpu_placeholder_type_copyin);
          is_valid_op = true;
          break;
        }
      // copyout apis
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoH:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoHAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoH_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoHAsync_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoH_v2_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoHAsync_v2_ptsz:
        {
	  gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, 
				       gpu_placeholder_type_copyout);
          is_valid_op = true;
          break;
        }
      // copy apis
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoD:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoA:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoD:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoA:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoH:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoA:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy2D:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DUnaligned:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy3D:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoDAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoAAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoHAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoD_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoDAsync_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoH_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoHAsync_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoD_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoA_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoA_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy2D_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DUnaligned_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DAsync_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy3D_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DAsync_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoA_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoAAsync_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyPeer:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyPeerAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DPeer:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DPeerAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoD_v2_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoA_v2_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoD_v2_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoA_v2_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoH_v2_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoA_v2_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy2D_v2_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DUnaligned_v2_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy3D_v2_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyPeer_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DPeer_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAsync_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoAAsync_v2_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoHAsync_v2_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoDAsync_v2_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DAsync_v2_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DAsync_v2_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyPeerAsync_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DPeerAsync_ptsz:
        {
	  gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, 
				       gpu_placeholder_type_copy);
          is_valid_op = true;
          break;
        }
      // kernel apis
      case CUPTI_DRIVER_TRACE_CBID_cuLaunch:
      case CUPTI_DRIVER_TRACE_CBID_cuLaunchGrid:
      case CUPTI_DRIVER_TRACE_CBID_cuLaunchGridAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuLaunchKernel:
      case CUPTI_DRIVER_TRACE_CBID_cuLaunchKernel_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuLaunchCooperativeKernel:
      case CUPTI_DRIVER_TRACE_CBID_cuLaunchCooperativeKernel_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuLaunchCooperativeKernelMultiDevice:
        {
          // Process previous activities
	  gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, 
				       gpu_placeholder_type_kernel);
          is_valid_op = true;
          if (cd->callbackSite == CUPTI_API_ENTER) {
            gpu_activity_channel_consume();

            // XXX(Keren): cannot parse this kind of kernel launch
            //if (cb_id != CUPTI_DRIVER_TRACE_CBID_cuLaunchCooperativeKernelMultiDevice)
            // CUfunction is the first param
            CUfunction function_ptr = *(CUfunction *)((CUfunction)cd->functionParams);
            func_ip = cupti_func_ip_resolve(function_ptr);
          }
          break;
        }
      default:
        break;
    }
    // If we have a valid operation and is not in the interval of a cuda/ompt runtime api
    if (is_valid_op && !cupti_runtime_api_flag && !ompt_runtime_api_flag) {
      if (cd->callbackSite == CUPTI_API_ENTER) {
        // A driver API cannot be implemented by other driver APIs, so we get an id
        // and unwind when the API is entered
        uint64_t correlation_id = atomic_fetch_add(&cupti_correlation_id, 1);
        cupti_correlation_id_push(correlation_id);
        cct_node_t *api_node = cupti_correlation_callback(correlation_id);

        gpu_op_ccts_t gpu_op_ccts;
        memset(&gpu_op_ccts, 0, sizeof(gpu_op_ccts));
        cct_addr_t api_frm;
        memset(&api_frm, 0, sizeof(cct_addr_t));

        hpcrun_safe_enter();

#if 0
        if (is_copy_op) {
          api_frm.ip_norm = gpu_driver_placeholders.gpu_copy_state.pc_norm;
          gpu_driver_ccts.copy_node = hpcrun_cct_insert_addr(api_node, &api_frm);
        } else if (is_copyin_op) {
          api_frm.ip_norm = gpu_driver_placeholders.gpu_copyin_state.pc_norm;
          gpu_driver_ccts.copyin_node = hpcrun_cct_insert_addr(api_node, &api_frm);
        } else if (is_copyout_op) {
          api_frm.ip_norm = gpu_driver_placeholders.gpu_copyout_state.pc_norm;
          gpu_driver_ccts.copyout_node = hpcrun_cct_insert_addr(api_node, &api_frm);
        } else if (is_alloc_op) {
          api_frm.ip_norm = gpu_driver_placeholders.gpu_alloc_state.pc_norm;
          gpu_driver_ccts.alloc_node = hpcrun_cct_insert_addr(api_node, &api_frm);
        } else if (is_delete_op) {
          api_frm.ip_norm = gpu_driver_placeholders.gpu_delete_state.pc_norm;
          gpu_driver_ccts.delete_node = hpcrun_cct_insert_addr(api_node, &api_frm);
        } else if (is_sync_op) {
          api_frm.ip_norm = gpu_driver_placeholders.gpu_sync_state.pc_norm;
          gpu_driver_ccts.sync_node = hpcrun_cct_insert_addr(api_node, &api_frm);
        } else if (is_kernel_op) {
          memset(&api_frm, 0, sizeof(cct_addr_t));
          api_frm.ip_norm = gpu_driver_placeholders.gpu_trace_state.pc_norm;
          gpu_driver_ccts.trace_node = hpcrun_cct_insert_addr(api_node, &api_frm);
          api_frm.ip_norm = gpu_driver_placeholders.gpu_kernel_state.pc_norm;
          gpu_driver_ccts.kernel_node = hpcrun_cct_insert_addr(api_node, &api_frm);

          cct_addr_t func_frm;
          memset(&func_frm, 0, sizeof(cct_addr_t));
          func_frm.ip_norm = func_ip;
          cct_node_t *cct_func = hpcrun_cct_insert_addr(gpu_driver_ccts.trace_node, &func_frm);
          hpcrun_cct_retain(cct_func);
        }
#else
	gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);
    if (is_kernel_op) {
      cct_addr_t func_frm;

      memset(&func_frm, 0, sizeof(cct_addr_t));

      func_frm.ip_norm = func_ip;

      cct_node_t *cct_func = 
	hpcrun_cct_insert_addr(gpu_driver_ccts.trace_node, &func_frm);
      hpcrun_cct_retain(cct_func);
    }
#endif

        hpcrun_safe_exit();

        // Generate notification entry
        cupti_correlation_channel_produce(
          cupti_correlation_channel_get(),
          correlation_id, 
          cupti_activity_channel_get(),
          &gpu_driver_ccts);

        PRINT("Driver push externalId %lu (cb_id = %u)\n", correlation_id, cb_id);
      } else if (cd->callbackSite == CUPTI_API_EXIT) {
        uint64_t correlation_id = cupti_correlation_id_pop();
        PRINT("Driver pop externalId %lu (cb_id = %u)\n", correlation_id, cb_id);
      }
    } else if (is_kernel_op && cupti_runtime_api_flag && cd->callbackSite == CUPTI_API_ENTER) {
      if (cupti_trace_node != NULL) {
        cct_addr_t func_frm;
        memset(&func_frm, 0, sizeof(cct_addr_t));
        func_frm.ip_norm = func_ip;
        cct_node_t *cct_func = hpcrun_cct_insert_addr(cupti_trace_node, &func_frm);
        hpcrun_cct_retain(cct_func);
      }
    } else if (is_kernel_op && ompt_runtime_api_flag && cd->callbackSite == CUPTI_API_ENTER) {
      cct_node_t *ompt_trace_node = ompt_trace_node_get();
      if (ompt_trace_node != NULL) {
        cct_addr_t func_frm;
        memset(&func_frm, 0, sizeof(cct_addr_t));
        func_frm.ip_norm = func_ip;
        cct_node_t *cct_func = hpcrun_cct_insert_addr(ompt_trace_node, &func_frm);
        hpcrun_cct_retain(cct_func);
      }
    }
  } else if (domain == CUPTI_CB_DOMAIN_RUNTIME_API) {
    // stop flag is only set if a driver or runtime api called
    cupti_stop_flag_set();
    cupti_correlation_channel_init();
    cupti_activity_channel_init();

    const CUpti_CallbackData *cd = (const CUpti_CallbackData *)cb_info;

    bool is_valid_op = false;
    bool is_kernel_op = false;
    switch (cb_id) {
      // FIXME(Keren): do not support memory allocate and free for
      // current CUPTI version
      //case CUPTI_RUNTIME_TRACE_CBID_cudaMalloc_v3020:
      //case CUPTI_RUNTIME_TRACE_CBID_cudaMallocPitch_v3020:
      //case CUPTI_RUNTIME_TRACE_CBID_cudaMallocArray_v3020:
      //case CUPTI_RUNTIME_TRACE_CBID_cudaMalloc3D_v3020:
      //case CUPTI_RUNTIME_TRACE_CBID_cudaMalloc3DArray_v3020:
      //  {
      //    cuda_state = cuda_placeholders.cuda_memalloc_state;
      //    is_valid_op = true;
      //    break;
      //  }
      // cuda synchronize apis
      case CUPTI_RUNTIME_TRACE_CBID_cudaEventSynchronize_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaStreamSynchronize_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaStreamSynchronize_ptsz_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaStreamWaitEvent_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaDeviceSynchronize_v3020:
      // cuda copy apis
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyPeer_v4000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyPeerAsync_v4000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3DPeer_v4000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3DPeerAsync_v4000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3D_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3DAsync_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3D_ptds_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3DAsync_ptsz_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3DPeer_ptds_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3DPeerAsync_ptsz_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2D_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToArray_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DToArray_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromArray_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DFromArray_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyArrayToArray_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DArrayToArray_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToSymbol_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromSymbol_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyAsync_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToArrayAsync_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromArrayAsync_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DAsync_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DToArrayAsync_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DFromArrayAsync_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToSymbolAsync_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromSymbolAsync_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy_ptds_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2D_ptds_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToArray_ptds_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DToArray_ptds_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromArray_ptds_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DFromArray_ptds_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyArrayToArray_ptds_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DArrayToArray_ptds_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToSymbol_ptds_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromSymbol_ptds_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyAsync_ptsz_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToArrayAsync_ptsz_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromArrayAsync_ptsz_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DAsync_ptsz_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DToArrayAsync_ptsz_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DFromArrayAsync_ptsz_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToSymbolAsync_ptsz_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromSymbolAsync_ptsz_v7000:
        {
          is_valid_op = true;
          break;
        }
      // cuda kernel apis
      case CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaLaunchKernel_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_ptsz_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaLaunchKernel_ptsz_v7000:
      #if CUPTI_API_VERSION >= 10
      case CUPTI_RUNTIME_TRACE_CBID_cudaLaunchCooperativeKernel_v9000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaLaunchCooperativeKernel_ptsz_v9000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaLaunchCooperativeKernelMultiDevice_v9000:
      #endif
        {
          is_valid_op = true;
          is_kernel_op = true;
          if (cd->callbackSite == CUPTI_API_ENTER) {
            // Process previous activities
            cupti_activity_channel_consume(cupti_activity_channel_get());
          }
          break;
        }
      default:
        break;
    }
    if (is_valid_op) {
      if (cd->callbackSite == CUPTI_API_ENTER) {
        // Enter a CUDA runtime api
        cupti_runtime_api_flag_set();
        uint64_t correlation_id = atomic_fetch_add(&cupti_correlation_id, 1);
        cupti_correlation_id_push(correlation_id);
        // We should make notification records in the api enter callback.
        // A runtime API must be implemented by driver APIs.
        // Though unlikely in most cases,
        // it is still possible that a cupti buffer is full and returned to the host
        // in the interval of a runtime api.
        cct_node_t *api_node = cupti_correlation_callback(correlation_id);

        gpu_op_ccts_t gpu_op_ccts;

        hpcrun_safe_enter();

#if 0
        memset(&gpu_driver_ccts, 0, sizeof(gpu_driver_ccts_t));
        cct_addr_t api_frm;
        memset(&api_frm, 0, sizeof(cct_addr_t));

        api_frm.ip_norm = gpu_driver_placeholders.gpu_copy_state.pc_norm;
        gpu_driver_ccts.copy_node = hpcrun_cct_insert_addr(api_node, &api_frm);
        api_frm.ip_norm = gpu_driver_placeholders.gpu_copyin_state.pc_norm;
        gpu_driver_ccts.copyin_node = hpcrun_cct_insert_addr(api_node, &api_frm);
        api_frm.ip_norm = gpu_driver_placeholders.gpu_copyout_state.pc_norm;
        gpu_driver_ccts.copyout_node = hpcrun_cct_insert_addr(api_node, &api_frm);
        api_frm.ip_norm = gpu_driver_placeholders.gpu_alloc_state.pc_norm;
        gpu_driver_ccts.alloc_node = hpcrun_cct_insert_addr(api_node, &api_frm);
        api_frm.ip_norm = gpu_driver_placeholders.gpu_delete_state.pc_norm;
        gpu_driver_ccts.delete_node = hpcrun_cct_insert_addr(api_node, &api_frm);
        api_frm.ip_norm = gpu_driver_placeholders.gpu_trace_state.pc_norm;
        gpu_driver_ccts.trace_node = hpcrun_cct_insert_addr(api_node, &api_frm);
        api_frm.ip_norm = gpu_driver_placeholders.gpu_kernel_state.pc_norm;
        gpu_driver_ccts.kernel_node = hpcrun_cct_insert_addr(api_node, &api_frm);
        api_frm.ip_norm = gpu_driver_placeholders.gpu_sync_state.pc_norm;
        gpu_driver_ccts.sync_node = hpcrun_cct_insert_addr(api_node, &api_frm);
#else
	gpu_op_ccts_insert(api_node, &gpu_op_ccts);
#endif
        
        hpcrun_safe_exit();

        cupti_trace_node = gpu_op_ccts.ccts[gpu_placeholder_type_trace];

        // Generate notification entry
        cupti_correlation_channel_produce(
          cupti_correlation_channel_get(),
          correlation_id, 
          cupti_activity_channel_get(),
          &gpu_driver_ccts);

        PRINT("Runtime push externalId %lu (cb_id = %u)\n", correlation_id, cb_id);
      } else if (cd->callbackSite == CUPTI_API_EXIT) {
        // Exit an CUDA runtime api
        cupti_runtime_api_flag_unset();
        uint64_t correlation_id = cupti_correlation_id_pop();
        cupti_trace_node = NULL;
        PRINT("Runtime pop externalId %lu (cb_id = %u)\n", correlation_id, cb_id);
      }
    } else {
      PRINT("Go through runtime with kernel_op %d, valid_op %d, \
        cuda_runtime %d\n", is_kernel_op, is_valid_op, cupti_runtime_api_flag);
    }
  }
}


