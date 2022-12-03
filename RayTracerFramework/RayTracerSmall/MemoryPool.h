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

