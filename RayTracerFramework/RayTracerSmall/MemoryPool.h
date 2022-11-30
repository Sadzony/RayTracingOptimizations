#pragma once
#include "MemoryDebugger.h"
#include <vector>
template<class T>
class MemoryPool
{
private:
	int m_poolMaxObjCount;
	size_t m_poolMaxByteSize;
	size_t m_objectSize;
	int m_objectCount;
	void* memoryPoolBlockStart;
public:
	MemoryPool(int poolMaxObjCount)
	{
		m_objectCount = 0;
		m_poolMaxObjCount = poolMaxObjCount;
		m_objectSize = sizeof(T);

		#ifdef _DEBUG
		m_objectSize += sizeof(Footer) + sizeof(Header);
		#endif // _DEBUG

		m_poolMaxByteSize = m_objectSize * m_poolMaxObjCount;

		//allocate m_poolMaxByteSize amount of memory
		memoryPoolBlockStart = (char*)malloc(m_poolMaxByteSize);

		#ifdef _DEBUG
		HeapManager::GetHeapByIndex((int)HeapID::Graphics)->AddBytes(m_poolMaxByteSize);
		#endif // _DEBUG
	}
	~MemoryPool()
	{
		ReleaseObjects();
		#ifdef _DEBUG
		HeapManager::GetHeapByIndex((int)HeapID::Graphics)->SubtractBytes(m_poolMaxByteSize);
		#endif // _DEBUG
	}
	void ReleaseObjects()
	{
		for (int i = 0; i < count() - 1; i++) {
			ReleaseLast();
		}
	}
	void ReleaseLast()
	{
		//get the last object and call its destructor
		T* obj = GetAt(count() - 1);
		obj->~T();
	}
	T* GetAt(int pos) {
		//different depending on whether in debug mode or not
		return nullptr;
	}
	int count() { return m_objectCount; }

	size_t GetObjectSize() { return m_objectSize; }

	size_t GetMaxByteSize() { return m_poolMaxByteSize;  }

	int GetMaxCount() { return m_poolMaxObjCount; }

	void* GetPoolMemBlock() { return memoryPoolBlockStart; }

	//list storing object pointers
	std::vector<void*> pointerList;
	void AddPointer(void* obj)
	{
		pointerList.push_back(obj);
		m_objectCount++;
	}

//new and delete overrides
public:
#ifdef _DEBUG
	//new operator only changes in debug mode
	static void* operator new (size_t size) 
	{
		return ::operator new(size, HeapID::Graphics);
	}
#endif
	//delete operator frees the allocated 
	static void operator delete(void* p, size_t size) 
	{
		//free the allocated memory pool block
		::operator delete(p);
	}

};

//Memory pool new and delete overloads
template<typename T>
void* operator new (size_t size, MemoryPool<T>* pool) 
{
	size_t requestedBytes = size;
	#ifdef _DEBUG
	requestedBytes += sizeof(Header) + sizeof(Footer);
	#endif

	if (pool->count() + 1 > pool->GetMaxCount()) {
		//error: pool is full
		return nullptr;
	}
	else if (requestedBytes != pool->GetObjectSize()) {
		//error: wrong object type
		return nullptr;
	}
	
	void* thisMemBlock = pool->GetPoolMemBlock();
	thisMemBlock = (void*)((char*)thisMemBlock + (pool->GetObjectSize() * pool->count()));
#ifdef _DEBUG
	Header* pHeader = (Header*)thisMemBlock; //header pointer is at the start of memory block
	pHeader->m_heap = HeapManager::GetHeapByIndex((int)HeapID::Graphics);
	pHeader->m_dataSize = size; //value of size in header equal to size of requested data 
	pHeader->m_totalDataSize = requestedBytes;
	pHeader->m_id = HeapID::Graphics;
	pHeader->checkValue = 0xDEED;


	//set up linked list
	Header* last = pHeader->m_heap->GetLast();
	last->next = pHeader;
	pHeader->previous = last;
	pHeader->m_heap->SetLast(pHeader);
	pHeader->next = nullptr;

	void* pFooterAddress = (void*)((char*)thisMemBlock + sizeof(Header) + size); //address of footer is past the header data and variable data
	Footer* pFooter = (Footer*)pFooterAddress;
	pFooter->m_id = HeapID::Graphics;
	pFooter->checkValue = 0xFEED;
	thisMemBlock = (void*)((char*)thisMemBlock + sizeof(Header));
#endif // DEBUG
	pool->AddPointer(thisMemBlock);
	return thisMemBlock;
}

template<typename T>
void operator delete (void* pMem, MemoryPool<T>* pool) 
{

}


