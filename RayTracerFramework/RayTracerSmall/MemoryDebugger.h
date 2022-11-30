#pragma once
#ifdef _DEBUG
#include <iostream>
#include <vector>



enum class HeapID //flag defining what heap the object is in.
{
	Default,
	Graphics,
	Heap, //this flag is used to define a heap object.
};

//forward declare
struct MemoryHeap;


//Header and footer structs for identifying allocated memory
struct Header {
	int m_dataSize;
	int m_totalDataSize;
	HeapID m_id;
	unsigned short int checkValue;
	MemoryHeap* m_heap;

	Header* previous;
	Header* next;
};
struct Footer {
	unsigned short int checkValue;
	HeapID m_id;
};



class HeapManager
{
//static methods
public:
	static void InitializeHeaps();
	static void Clear(int index);
	static void CleanUp();

	static MemoryHeap* GetHeapByIndex(int index) { return heaps[index]; }
	static bool initialized;
protected:
	static MemoryHeap* heaps [2];
};
//heap definitions
class MemoryHeap
{
public:
	Header* GetLast() { return last; }
	void SetLast(Header* p_last) { last = p_last; }
	void AddBytes(int p_bytes) { curBytesAllocated += p_bytes; bytesPlusHeaderFooter += p_bytes + sizeof(Header) + sizeof(Footer); }
	void SubtractBytes(int p_bytes) { curBytesAllocated -= p_bytes; bytesPlusHeaderFooter -= p_bytes + sizeof(Header) + sizeof(Footer); }
	bool WalkTheHeap();
protected:
	int curBytesAllocated = 0;
	int bytesPlusHeaderFooter = 0;
	Header* last;
};

//global new and delete overrides
void* operator new (size_t size);
void operator delete (void* pMem);

//new operator on non-default heaps
void* operator new (size_t size, HeapID heapType);


//tracker specific operator
void* operator new (size_t size, bool isHeap);
void operator delete (void* pMem, bool isHeap);

//get footer and get header
Header* GetHeaderPntr(void* pntr);
Footer* GetFooterPntr(void* pntr);
void* GetAddressFromHeader(Header* header);

#endif // DEBUG