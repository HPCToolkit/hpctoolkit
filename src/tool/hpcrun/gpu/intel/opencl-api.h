#include <CL/cl.h>
#include <hpcrun/cct/cct.h> // cct_node_t

typedef struct profilingData
{
	cl_ulong queueTime;
	cl_ulong submitTime;
	cl_ulong startTime;
	cl_ulong endTime;
	size_t size;
	bool fromHostToDevice;
	bool fromDeviceToHost;
} profilingData;

cct_node_t* createNode();

void updateNodeWithKernelProfileData(cct_node_t* cct_node, profilingData *pd);

void updateNodeWithMemTransferProfileData(cct_node_t* cct_node, profilingData *pd);
