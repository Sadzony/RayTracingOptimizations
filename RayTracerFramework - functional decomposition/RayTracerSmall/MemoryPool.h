#pragma once
#include "MemoryDebugger.h"
#include <vector>
#include <algorithm>

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
	std::vector<T*> objects;
	MemoryPool(int poolMaxObjCount)
	{
		m_objectCount = 0;
		m_poolMaxObjCount = poolMaxObjCount;
		m_objectSize = sizeof(T);

		//initialize the vector with nullptrs
		for (int i = 0; i < poolMaxObjCount; i++)
		{
			objects.push_back(nullptr);
		}

		#ifdef _DEBUG
		m_objectSize += sizeof(Header) + sizeof(Footer);
		#endif // _DEBUG

		m_poolMaxByteSize = m_objectSize * m_poolMaxObjCount;

		//allocate x times of object size amount of memory. Calloc takes longer than malloc but is ensures that the memory is always one single block.
		memoryPoolBlockStart = calloc(m_poolMaxObjCount, m_objectSize);

		//setup headers and footers in debug mode
		#ifdef _DEBUG

		for (int i = 0; i < poolMaxObjCount; i++) {

			//move pointer to memory positions of all the pool objects
			char* pMem = (char*)memoryPoolBlockStart + (m_objectSize * i);

			Header* pHeader = (Header*)pMem; //header pointer is at the start of memory block
			pHeader->m_dataSize = sizeof(T); //value of size in header equal to size of requested data 
			pHeader->m_totalDataSize = m_objectSize;
			pHeader->m_id = HeapID::Graphics;
			pHeader->m_heap = HeapManager::GetHeapByIndex((int)HeapID::Graphics);
			pHeader->checkValue = 0xDEED;

			Header* last = pHeader->m_heap->GetLast();
			last->next = pHeader;
			pHeader->previous = last;
			pHeader->m_heap->SetLast(pHeader);
			pHeader->next = nullptr;

			void* pFooterAddress = pMem + sizeof(Header) + sizeof(T); //address of footer is past the header data and variable data
			Footer* pFooter = (Footer*)pFooterAddress;
			pFooter->m_id = HeapID::Default;
			pFooter->checkValue = 0xFEED;
		}
		HeapManager::GetHeapByIndex((int)HeapID::Graphics)->AddBytes(m_poolMaxByteSize);
		#endif // _DEBUG
	}
	~MemoryPool()
	{
		ReleaseObjects();
		#ifdef _DEBUG
		for (int i = m_poolMaxObjCount-1; i >= 0; i--)
		{
			char* pMem = (char*)memoryPoolBlockStart + (m_objectSize * i);
			Header* pHeader = (Header*)pMem;
			//fix linked list
			if (pHeader->next != nullptr) {
				pHeader->next->previous = pHeader->previous;
			}

			//if header is in between headers
			if (pHeader->previous != nullptr && pHeader->next != nullptr) {
				pHeader->previous->next = pHeader->next;
			}
			//if header is last
			else {
				pHeader->previous->next = nullptr;
				pHeader->m_heap->SetLast(pHeader->previous);
			}
		}
		#endif // _DEBUG
		objects.clear();
		free(memoryPoolBlockStart);
		#ifdef _DEBUG
		HeapManager::GetHeapByIndex((int)HeapID::Graphics)->SubtractBytes(m_poolMaxByteSize);
		#endif // _DEBUG
	}
	void ReleaseObjects()
	{
		int originalCount = count();
		//release all objects
		for (int i = 0; i < originalCount; i++) {
			ReleaseLast();
		}
	}
	void ReleaseLast()
	{
		//get the last object and call its destructor
		ReleaseAt(m_objectCount - 1);
		
	}
	void ReleaseAt(int pos) 
	{
		objects.at(pos) = nullptr;
		Decrement();
	}
	T* GetAt(int pos) const {
		return objects.at(pos);
	}
	int count() const { return m_objectCount; }

	size_t GetObjectSize() const { return m_objectSize; }

	size_t GetMaxByteSize() const { return m_poolMaxByteSize;  }

	int GetMaxCount() const { return m_poolMaxObjCount; }

	void* GetPoolMemBlock() const { return memoryPoolBlockStart; }

	void Increment() { m_objectCount++; }
	void Decrement() { m_objectCount--; }

	

//new and delete overrides
public:
#ifdef _DEBUG
	//new operator only changes in debug mode
	void* operator new (size_t size) 
	{
		return ::operator new(size, HeapID::Graphics);
	}
#endif
};

//Memory pool new and delete overloads
template<typename T>
void* operator new (size_t size, MemoryPool<T>* pool) 
{
	size_t requestedBytes = size;
	#ifdef _DEBUG
	requestedBytes += sizeof(Header) + sizeof(Footer);
	#endif // DEBUG


	if (pool->count() == pool->GetMaxCount()) {
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
	thisMemBlock = (void*)((char*)thisMemBlock + sizeof(Header));
	#endif // _DEBUG

	pool->objects.at(pool->count()) = (T*)thisMemBlock;
	pool->Increment();

	return thisMemBlock;
}
template<typename T>
void* operator new (size_t size, MemoryPool<T> pool)
{
	size_t requestedBytes = size;
#ifdef _DEBUG
	requestedBytes += sizeof(Header) + sizeof(Footer);
#endif // DEBUG


	if (pool.count() == pool.GetMaxCount()) {
		//error: pool is full
		return nullptr;
	}
	else if (requestedBytes != pool.GetObjectSize()) {
		//error: wrong object type
		return nullptr;
	}

	void* thisMemBlock = pool.GetPoolMemBlock();
	thisMemBlock = (void*)((char*)thisMemBlock + (pool->GetObjectSize() * pool.count()));

#ifdef _DEBUG
	thisMemBlock = (void*)((char*)thisMemBlock + sizeof(Header));
#endif // _DEBUG

	pool.objects.at(pool.count()) = (T*)thisMemBlock;
	pool.Increment();

	return thisMemBlock;
}

template<typename T>
void operator delete (void* pMem, MemoryPool<T>* pool)
{
	//release the object rather than deleting it
	T* obj = (T*)pMem;
	auto it = find(pool->objects.begin(), pool->objects.end(), obj);
	if (it != pool->objects.end()) {
		int index = it - pool->objects.begin();
		pool->ReleaseAt(index);
	}
}
template<typename T>
void operator delete (void* pMem, MemoryPool<T> pool)
{
	//release the object rather than deleting it
	T* obj = (T*)pMem;
	auto it = find(pool.objects.begin(), pool.objects.end(), obj);
	if (it != pool.objects.end()) {
		int index = it - pool.objects.begin();
		pool.ReleaseAt(index);
	}
}