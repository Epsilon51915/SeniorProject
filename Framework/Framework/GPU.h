/*
*	The GPU namespace will contain access to all gpu operations
*	Kept in a namespace for organizational reasons as making a GPU class is not necessary
*/


namespace gpu
{
	void* gpuMalloc(size_t bytes);
	void* gpuMalloc(size_t bytes, size_t alignment);
	void* gpuHostToDevicePointer(void* ptr);
	void gpuMemCopy();
	void gpuFree(void* ptr);
	void gpuDispatch();
	void gpuDrawIndexed();


	void copyMemory();
	void copyMemoryToTexture();
	void copyTextureToMemory();
}