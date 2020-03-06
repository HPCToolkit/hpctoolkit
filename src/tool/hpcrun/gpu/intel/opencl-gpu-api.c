createNodeForMemTransfer()
{
	// open question: What should be used as node key? should I use device_id + randomString?
	// get device info using the command-queue(commandqueues are unique for a device)
	// 2 types of memory operations, host-to-device and device-to-host:
	// host-to-device() clEnqueueWriteBuffer will provide info related to size of data
	// device-to-host() clEnqueueReadBuffer will provide info related to size of data

}

updateNodeWithMemTransferProfileData()
{

}

createNodeForKernel()
{
	// unwind call stack, remove all frames not related to the kernel, and create a node for this kernel

}

updateNodeWithKernelProfileData()
{
	// add profilingData info to the node correspoding to the kernel (does node need a kernelname as a unique key??)	
}
