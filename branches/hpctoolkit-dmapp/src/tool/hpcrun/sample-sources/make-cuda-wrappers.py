#!/usr/local/bin/python 
# -*- python -*-
#   
#   HPCToolkit MPI Profiler
#   this script is adapted from mpiP MPI Profiler ( http://mpip.sourceforge.net/ )
#
#   Please see COPYRIGHT AND LICENSE information at the end of this file.
#
#
#   make-wrappers.py -- parse the mpi prototype file and generate a
#     series of output files, which include the wrappers for profiling
#     layer and other data structures.
#
#   $Id: make-wrappers.py 442 2010-03-03 17:18:04Z chcham $
#

import sys
import string
import os
import copy
import re
import time
import getopt
import socket
import pdb


driverSkipList = [
'cuCtxCreate_v2',
'cuCtxDestroy_v2',
'cuMemcpyHtoD_v2',
'cuMemcpyDtoH_v2',
'cuMemcpyHtoDAsync_v2',
'cuMemcpyDtoHAsync_v2',
'cuStreamCreate',
'cuStreamSynchronize',
'cuStreamDestroy_v2',
'cuEventSynchronize',
'cuLaunchGridAsync',
'cuLaunchKernel']


runtimeSkipList = [
'cudaDeviceSynchronize',
'cudaThreadSynchronize',
'cudaStreamCreate',
'cudaStreamDestroy',
'cudaStreamWaitEvent',
'cudaStreamSynchronize',
'cudaEventSynchronize',
'cudaConfigureCall',
'cudaLaunch',
'cudaMalloc',
'cudaMallocArray',
'cudaFree',
'cudaFreeArray',
'cudaMemcpy',
'cudaMemcpy2D',
'cudaMemcpyAsync',
'cudaMemcpyToArray',
'cudaMemcpyToArrayAsync',
'cudaMalloc3D',
'cudaMalloc3DArray',
'cudaMemcpy3D',
'cudaMemcpy3DPeer',
'cudaMemcpy3DAsync',
'cudaMemcpy3DPeerAsync',
'cudaMemcpyPeer',
'cudaMemcpyFromArray',
'cudaMemcpyArrayToArray',
'cudaMemcpy2DToArray',
'cudaMemcpy2DFromArray',
'cudaMemcpy2DArrayToArray',
'cudaMemcpyToSymbol',
'cudaMemcpyFromSymbol',
'cudaMemcpyPeerAsync',
'cudaMemcpyFromArrayAsync',
'cudaMemcpy2DAsync',
'cudaMemcpy2DToArrayAsync',
'cudaMemcpy2DFromArrayAsync',
'cudaMemcpyToSymbolAsync',
'cudaMemcpyFromSymbolAsync',
'cudaMemset',
'cudaMemset2D',
'cudaMemset3D',
'cudaMemsetAsync',
'cudaMemset2DAsync',
'cudaMemset3DAsync']




def WritecuDriverFunctionPointerTable(file, funcNames):
# a table like this:
#cuDriverFunctionPointer_t cuDriverFunctionPointer[] = {
#    {0, "cuStreamCreate"},
#    {0, "cuStreamDestroy"},
#    ...
#};
    fp = open(file,'w')
    fp.write('''
// GENERATED FILE DON'T EDIT
#include "gpu_blame-cuda-driver-header.h"
#include<cuda.h>
#include<cuda_runtime_api.h>
''')
    fp.write('cuDriverFunctionPointer_t cuDriverFunctionPointer[] = {\n')
    for name in funcNames:
        fp.write('\t {{.generic = (void*)0},"' + name[1] + '"},\n')
    fp.write('};\n')
    fp.close();

def WritecuRuntimeFunctionPointerTable(file, funcNames):
# a table like this:
#cudaRuntimeFunctionPointer_t  cudaRuntimeFunctionPointer[] = {
#    {0, "cudaThreadSynchronize"},
#    ...
#};
    fp = open(file,'w')
    fp.write('''
// GENERATED FILE DON'T EDIT
#include<cuda.h>
#include<cuda_runtime_api.h>
#include "gpu_blame-cuda-runtime-header.h"
''')
    fp.write('cudaRuntimeFunctionPointer_t  cudaRuntimeFunctionPointer[] = {\n')
    for name in funcNames:
        fp.write('\t {{.generic = (void*)0},"' + name[1] + '"},\n')
    fp.write('};\n')
    fp.close();


def FuncNameToCapitalizedEnum(name):
# convert cuStreamCreate to CU_STREAM_CREATE
    result = ''
    for letter in name:
        if letter.isupper():
            result = result + '_'
        result = result + letter.upper()
    return result


def FuncNameToEnum(name):
    return name  + 'Enum'



def WriteDriverFunctionPointerHeader(file, funcSig):
#Produce struct like this:
#typedef struct cuDriverFunctionPointer {
#    union {
#        CUresult(*generic) (void);
#        CUresult(*cuStreamCreateReal) (CUstream * phStream, unsigned int Flags);
#        CUresult(*cuStreamDestroyReal) (CUstream hStream);
#        CUresult(*cuStreamSynchronizeReal) (CUstream hStream);
#        CUresult(*cuEventSynchronizeReal) (CUevent event);
#    };
#    const char *functionName;
#} cuDriverFunctionPointer_t;


    fp = open(file,'w')
    fp.write('''
// GENERATED FILE DON'T EDIT
#ifndef __CU_DRIVER_HEADER_H__
#define __CU_DRIVER_HEADER_H__
#include<cuda.h>
#include<cuda_runtime_api.h>
typedef struct cuDriverFunctionPointer {
    union {
	 void* generic;
''')

    for sig in funcSig:
        fp.write('\t' + sig[0] + '(*' + sig[1] + 'Real) (' + sig[2] + ');\n' )

    fp.write(
'''    };
    const char *functionName;
} cuDriverFunctionPointer_t;
''')

# create enum like this:
#enum cuDriverAPIIndex {
#    cuStreamCreateEnum,
#    cuStreamDestroyEnum,
#    ...
#    CU_MAX_APIS
#};

    fp.write('''
enum cuDriverAPIIndex {
''')
    for sig in funcSig:
        fp.write('\t' + FuncNameToEnum(sig[1]) + ',\n' )

    fp.write('''
CU_MAX_APIS
};
extern cuDriverFunctionPointer_t cuDriverFunctionPointer[CU_MAX_APIS];
''')

    fp.write('#endif\n')
    fp.close();



def WriteRuntimeFunctionPointerHeader(file, funcSig):
#Produce struct like this:
#typedef struct cudaRuntimeFunctionPointer {
#    union {
#        cudaError_t(*generic) (void);
#        cudaError_t(*cudaThreadSynchronizeReal) (void);
#    };
#    const char *functionName;
#} cudaRuntimeFunctionPointer_t;

    fp = open(file,'w')
    fp.write('''
// GENERATED FILE DON'T EDIT
#ifndef __CUDA_RUNTIME_HEADER_H__
#define __CUDA_RUNTIME_HEADER_H__
#include<cuda.h>
#include<cuda_runtime_api.h>
typedef struct cudaRuntimeFunctionPointer {
    union {
	 void* generic;
''')

    for sig in funcSig:
        fp.write('\t' + sig[0] +  '(*' + sig[1] + 'Real) (' + sig[2] + ');\n' )

    fp.write(
'''    };
    const char *functionName;
} cudaRuntimeFunctionPointer_t;
''')

# create enum like this:
#enum cudaRuntimeAPIIndex {
#    cudaThreadSynchronizeEnum,
#    cudaStreamSynchronizeEnum,
#    cudaDeviceSynchronizeEnum,
#    ...
#    CUDA_MAX_APIS
#};

    fp.write('''
enum cudaRuntimeAPIIndex{
''')
    for sig in funcSig:
        fp.write('\t' + FuncNameToEnum(sig[1]) + ',\n' )

    fp.write('''
CUDA_MAX_APIS
};
extern cudaRuntimeFunctionPointer_t cudaRuntimeFunctionPointer[CUDA_MAX_APIS];
''')

    fp.write('#endif\n')
    fp.close();



def WriteDriverFunctionWrapper(file, funcSig):
    fp = open(file,'w')
    fp.write('''
// GENERATED FILE DON'T EDIT
#include <stdbool.h>
#include <hpcrun/thread_data.h>
#include <monitor.h>
#include<cuda.h>
#include "gpu_blame-cuda-driver-header.h"
extern bool hpcrun_is_safe_to_sync(const char* fn);
''')

    for sig in funcSig:
        #skip the manually done ones
        if sig[1] in driverSkipList: continue

        fp.write('\t' + sig[0] +  sig[1] + ' (' + sig[2] + ') {\n' )
	fp.write('if (! hpcrun_is_safe_to_sync(__func__)) {')
	fp.write('    return cuDriverFunctionPointer[' +FuncNameToEnum(sig[1]) + '].' + sig[1] + 'Real(')
        args = sig[2].split(',')
        first = True
        for argTypeName in args:
            if not first:
                fp.write(', ')
            else:
                first = False
            param = argTypeName.split()[-1].split('*')[-1]
            if param.strip() != "void":
                fp.write(param)
            

        fp.write( ');\n')
	fp.write('}\n')
	fp.write('TD_GET(gpu_data.is_thread_at_cuda_sync) = true;\n')
	fp.write('monitor_disable_new_threads();\n')
        #fp.write('printf("\\n%s on","' +sig[1] +'");fflush(stdout);')
	fp.write('CUresult ret = cuDriverFunctionPointer[' +FuncNameToEnum(sig[1]) + '].' + sig[1] + 'Real(')

        args = sig[2].split(',')
        first = True
        for argTypeName in args:
            if not first:
                fp.write(', ')
            else:
                first = False
            param = argTypeName.split()[-1].split('*')[-1]
            if param.strip() != "void":
                fp.write(param)
            

        fp.write( ');\n')
	fp.write('monitor_enable_new_threads();\n')
	fp.write('TD_GET(gpu_data.is_thread_at_cuda_sync) = false;\n')
        #fp.write('printf("\\n%s off","' +sig[1] +'");fflush(stdout);')
	fp.write('return ret;\n')
        fp.write('}\n')
#    fp.write('''
##endif
#''')  
    fp.close();




def WriteRuntimeFunctionWrapper(file, funcSig):
    fp = open(file,'w')
    fp.write('''
// GENERATED FILE DON'T EDIT
#include <stdbool.h>
#include <hpcrun/thread_data.h>
#include <monitor.h>
#include<cuda.h>
#include<cuda_runtime_api.h>
#include "gpu_blame-cuda-runtime-header.h"
extern bool hpcrun_is_safe_to_sync(const char* fn);
''')

    for sig in funcSig:
        #skip the manually done ones
        if sig[1] in runtimeSkipList: continue

        fp.write('\t' + sig[0] +   sig[1] + ' (' + sig[2] + ') {\n' )
	fp.write('if (! hpcrun_is_safe_to_sync(__func__)) {')
	fp.write('    return cudaRuntimeFunctionPointer[' +FuncNameToEnum(sig[1]) + '].' + sig[1] + 'Real(')
        args = sig[2].split(',')
        first = True
        for argTypeName in args:
            if not first:
                fp.write(', ')
            else:
                first = False
            param = argTypeName.split()[-1].split('*')[-1]
            if param.strip() != "void":
                fp.write(param)
            

        fp.write( ');\n')
	fp.write('}\n')
        fp.write('TD_GET(gpu_data.is_thread_at_cuda_sync) = true;\n')
	fp.write('monitor_disable_new_threads();\n')
        #fp.write('printf("\\n%s on","' +sig[1] +'");')
        fp.write('cudaError_t ret = cudaRuntimeFunctionPointer[' +FuncNameToEnum(sig[1]) + '].' + sig[1] + 'Real(') 
 
        args = sig[2].split(',')
        first = True
        for argTypeName in args:
            if not first:
                fp.write(', ')
            else:
                first = False
            param = argTypeName.split()[-1].split('*')[-1]
            if param.strip() != "void":
                fp.write(param)
            
 
        fp.write( ');\n')
	fp.write('monitor_enable_new_threads();\n')
        fp.write('TD_GET(gpu_data.is_thread_at_cuda_sync) = false;\n')
        #fp.write('printf("\\n%s off","' +sig[1] +'");')
        fp.write('return ret;\n')
        fp.write('}\n')
    fp.close();









#cuPattern = '\s*(CUresult[\s\n]+)(CUDAAPI[\s\n]+)(cu[a-zA-Z0-9_]*[\s\n]*)\(([^;]*)\)[\s\n]*;'
#cudaPattern = '\s*extern[\s\n]+__host__[\s\n]+(cudaError_t[\s\n]+)(CUDARTAPI[\s\n]+)(cuda[a-zA-Z0-9_]*[\s\n]*)\(([^;]*)\)[\s\n]*;'


cuPattern = '\s*(CUresult[\s\n]+)(cu[a-zA-Z0-9_]*[\s\n]*)\(([^;]*)\)[\s\n]*;'
cudaPattern = '\s*extern[\s\n]+(cudaError_t[\s\n]+)(cuda[a-zA-Z0-9_]*[\s\n]*)\(([^;]*)\)[\s\n]*;'




inFile = open(sys.argv[2]).read()

generatedHeaderFile = sys.argv[3]
generatedTableFile = sys.argv[4]
generatedWrapperFile = sys.argv[5]


if sys.argv[1] == 'driver':
	lines = re.finditer(cuPattern,inFile, re.MULTILINE)
elif sys.argv[1] == 'runtime':
	lines = re.finditer(cudaPattern,inFile, re.MULTILINE)
else:
	print 'Invalid pattern'
	exit(-1)


defaultValue = re.compile('__dv\s*\(.*\)')

signatures = []
for line in lines:
    funcName = line.group(2)
    funcPrefix = line.group(1) 
    funcArgs = line.group(3)
    noDefaultArgs = defaultValue.sub('',funcArgs)
    #print p.group(1), p.group(2), p.group(3), p.group(4), p.group(5), '(', n, ')'
    args = noDefaultArgs.split(',')
    #print funcPrefix, funcName, '(' , noDefaultArgs, ')'
    for argTypeName in args:
        last = argTypeName.split()[-1]
	last = last.split('*')[-1] 
    	#print last
    signatures.append((funcPrefix, funcName, noDefaultArgs))

if sys.argv[1] == 'driver':
   WriteDriverFunctionPointerHeader(generatedHeaderFile, signatures) 
   WritecuDriverFunctionPointerTable(generatedTableFile, signatures)
   WriteDriverFunctionWrapper(generatedWrapperFile, signatures)
elif sys.argv[1] == 'runtime':
   WriteRuntimeFunctionPointerHeader(generatedHeaderFile, signatures) 
   WritecuRuntimeFunctionPointerTable(generatedTableFile, signatures)
   WriteRuntimeFunctionWrapper(generatedWrapperFile, signatures)

