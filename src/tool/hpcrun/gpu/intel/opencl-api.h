#include <CL/cl.h>
#include <hpcrun/cct/cct.h> // cct_node_t

struct profilingData
{
	cl_ulong queueTime;
	cl_ulong submitTime;
	cl_ulong startTime;
	cl_ulong endTime;
	size_t size;
	bool fromHostToDevice;
	bool fromDeviceToHost;
};

cct_node_t* createNode();

void updateNodeWithTimeProfileData(cct_node_t* cct_node, struct profilingData *pd);

void updateNodeWithMemTransferProfileData(cct_node_t* cct_node, struct profilingData *pd);
