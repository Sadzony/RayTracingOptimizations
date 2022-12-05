#pragma once
#include "MemoryDebugger.h"

//custom std::destroy_at since its not available??
template <typename T>
constexpr void destroy_at(T* p)
{
	p->~T();
}

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
		m_objectSize += sizeof(Header) + sizeof(Footer);
		#endif // _DEBUG

		m_poolMaxByteSize = m_objectSize * m_poolMaxObjCount;

		//allocate m_poolMaxByteSize amount of memory
		memoryPoolBlockStart = malloc(m_poolMaxByteSize);

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
		free(memoryPoolBlockStart);
		#ifdef _DEBUG
		HeapManager::GetHeapByIndex((int)HeapID::Graphics)->SubtractBytes(m_poolMaxByteSize);
		#endif // _DEBUG
	}
	void ReleaseObjects()
	{
		//release all objects
		for (int i = 0; i < count() - 1; i++) {
			ReleaseLast();
		}
	}
	void ReleaseLast()
	{
		//get the last object and call its destructor
		T* obj = GetAt(m_objectCount - 1);
		destroy_at(obj);
		Decrement();
		
	}
	void ReleaseAt(int pos) 
	{
		T* obj = GetAt(pos);
		destroy_at(obj);
		Decrement();
	}
	T* GetAt(int pos) {
		
		if (pos > m_objectCount) {
			//error - index out of range
			return nullptr;
		}
		size_t bytesToMove = pos * m_objectSize;
		void* mempntr = (void*)((char*)memoryPoolBlockStart + bytesToMove);
		#ifdef _DEBUG
		mempntr = (void*)((char*)mempntr + sizeof(Header));
		#endif // DEBUG

		T* object = (T*)mempntr;
		return object;
	}
	int count() { return m_objectCount; }

	size_t GetObjectSize() { return m_objectSize; }

	size_t GetMaxByteSize() { return m_poolMaxByteSize;  }

	int GetMaxCount() { return m_poolMaxObjCount; }

	void* GetPoolMemBlock() { return memoryPoolBlockStart; }

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
	thisMemBlock = (void*)((char*)thisMemBlock + sizeof(Header));
	#endif // _DEBUG

	
	pool->Increment();

	return thisMemBlock;
}

template<typename T>
void operator delete (void* pMem, MemoryPool<T>* pool) 
{
	//release the object rather than deleting it
	T* obj = (T*)pMem;
	destroy_at(obj);
}

