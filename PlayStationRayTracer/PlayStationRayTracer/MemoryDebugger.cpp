#ifdef _DEBUG

#include "MemoryDebugger.h"

//initialization of a static variables
MemoryHeap* HeapManager::heaps[2];
bool HeapManager::initialized = false;

//HeapManager definitions
void HeapManager::InitializeHeaps()
{
    MemoryHeap* heap = new (true) MemoryHeap(); //creates a new heap with isHead set to true (default heap)
    Header* heapHeader = GetHeaderPntr(heap);
    heapHeader->m_heap = heap;
    heap->SetLast(heapHeader);
    heap->AddBytes(sizeof(MemoryHeap));
    HeapManager::heaps[0] = heap;

    heap = new (true) MemoryHeap(); //creates a graphics heap
    heapHeader = GetHeaderPntr(heap);
    heapHeader->m_heap = heap;
    heap->SetLast(heapHeader);
    heap->AddBytes(sizeof(MemoryHeap));
    HeapManager::heaps[1] = heap;


    HeapManager::initialized = true;

}

//deletes contents of the heap
void HeapManager::Clear(int index)
{
    MemoryHeap* heap = HeapManager::GetHeapByIndex(index);
    Header* heapHead = GetHeaderPntr(heap);
    while (heap->GetLast() != heapHead)
    {

        //delete data at header
        delete (GetAddressFromHeader(heap->GetLast()));
    }
}
void HeapManager::CleanUp() 
{
    //find count of heaps in the app
    int count = sizeof(HeapManager::heaps) / sizeof(HeapManager::heaps[0]); //the array will always be fully initialized so count can be found using byte math
    for (int i = 0; i < count; i++)
    {
        // delete all contents of the heap first
        Clear(i);
        //then delete heap
        operator delete(heaps[i], true); //delete every heap with isHeap set to true
    }
}
bool MemoryHeap::WalkTheHeap()
{
    //initialize values
    int curBytes = 0;
    int curTotalBytes = 0;
    int variableAmount = 0;
    Header* currentHeaderOfInterest = GetHeaderPntr(this);
    Footer* currentFooterOfInterest = GetFooterPntr(this);
    void* currentPntrOfInterest = this;
    curBytes += currentHeaderOfInterest->m_dataSize;
    curTotalBytes += currentHeaderOfInterest->m_totalDataSize;
    variableAmount++;
    unsigned short int correctHeader = 0xDEED;
    unsigned short int correctFooter = 0xFEED;
    if (currentHeaderOfInterest->checkValue != correctHeader || currentFooterOfInterest->checkValue != correctFooter) {
        std::cout << "Error was detected on the heap pointer " << std::endl;
        return false;
    }

    //iterate over the heap and check the checkValue of header and footer.
    while (currentHeaderOfInterest->next != nullptr)
    {
        variableAmount++;
        currentHeaderOfInterest = currentHeaderOfInterest->next;
        currentPntrOfInterest = GetAddressFromHeader(currentHeaderOfInterest);
        currentFooterOfInterest = GetFooterPntr(currentPntrOfInterest);
        curBytes += currentHeaderOfInterest->m_dataSize;
        curTotalBytes += currentHeaderOfInterest->m_totalDataSize;
        //if any checkValues is wrong, return false and print info
        if (currentHeaderOfInterest->checkValue != correctHeader || currentFooterOfInterest->checkValue != correctFooter) {
            std::cout << "Error was detected on the position " << variableAmount << " element in the heap, at the address: " << currentPntrOfInterest << std::endl;
            return false;
        }
    }
    std::cout << "The heap was correct. There are " << curTotalBytes << " bytes allocated " << " to " << variableAmount << " variables. " <<
        curTotalBytes - curBytes << " of these bytes are occupied by header and footer data." << std::endl << "If header/footer info were removed, there would be " << curBytes << " bytes allocated." << std::endl;    
    return true;
}


//definitions for new and delete overrides
void* operator new(size_t size)
{


    if (!HeapManager::initialized) HeapManager::InitializeHeaps();


    size_t nRequestedBytes = size + sizeof(Header) + sizeof(Footer); //requested size plus the size of header and footer
    char* pMem = (char*)malloc(nRequestedBytes); //allocate memory + header and footer

    Header* pHeader = (Header*)pMem; //header pointer is at the start of memory block
    pHeader->m_dataSize = size; //value of size in header equal to size of requested data 
    pHeader->m_totalDataSize = nRequestedBytes;
    pHeader->m_id = HeapID::Default;
    pHeader->m_heap = HeapManager::GetHeapByIndex((int)HeapID::Default);
    pHeader->checkValue = 0xDEED;

    //set up linked list
    Header* last = pHeader->m_heap->GetLast();
    last->next = pHeader;
    pHeader->previous = last;
    pHeader->m_heap->SetLast(pHeader);
    pHeader->next = nullptr;
    

    pHeader->m_heap->AddBytes(size);

    void* pFooterAddress = pMem + sizeof(Header) + size; //address of footer is past the header data and variable data
    Footer* pFooter = (Footer*)pFooterAddress;
    pFooter->m_id = HeapID::Default;
    pFooter->checkValue = 0xFEED;

    void* pStartMemBlock = pMem + sizeof(Header);
    return pStartMemBlock;
}

void operator delete(void* pMem)
{
    Header* pHeader = GetHeaderPntr(pMem);
    Footer* pFooter = GetFooterPntr(pMem);

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
    pHeader->m_heap->SubtractBytes(pHeader->m_dataSize);

    

    free(pHeader);
}

void* operator new(size_t size, HeapID heapType)
{
    if (heapType == HeapID::Default)
        return ::operator new(size);
    else if (heapType == HeapID::Heap)
        return ::operator new(size, true);
    else {
        if (!HeapManager::initialized) HeapManager::InitializeHeaps();
       
        size_t nRequestedBytes = size + sizeof(Header) + sizeof(Footer); //requested size plus the size of header and footer
        char* pMem = (char*)malloc(nRequestedBytes); //allocate memory + header and footer

        Header* pHeader = (Header*)pMem; //header pointer is at the start of memory block
        pHeader->m_heap = HeapManager::GetHeapByIndex((int)heapType);
        pHeader->m_dataSize = size; //value of size in header equal to size of requested data 
        pHeader->m_totalDataSize = nRequestedBytes;
        pHeader->m_id = heapType;
        pHeader->checkValue = 0xDEED;


        //set up linked list
        Header* last = pHeader->m_heap->GetLast();
        last->next = pHeader;
        pHeader->previous = last;
        pHeader->m_heap->SetLast(pHeader);
        pHeader->next = nullptr;

        pHeader->m_heap->AddBytes(size);

        void* pFooterAddress = pMem + sizeof(Header) + size; //address of footer is past the header data and variable data
        Footer* pFooter = (Footer*)pFooterAddress;
        pFooter->m_id = heapType;
        pFooter->checkValue = 0xFEED;

        void* pStartMemBlock = pMem + sizeof(Header);
        return pStartMemBlock;
    }
}

//heap specific override
void* operator new(size_t size, bool isHeap)
{
    if (!isHeap) {
        return ::operator new(size);
    }
    else {
        size_t nRequestedBytes = size + sizeof(Header) + sizeof(Footer); //requested size plus the size of header and footer
        char* pMem = (char*)malloc(nRequestedBytes); //allocate memory + header and footer

        Header* pHeader = (Header*)pMem; //header pointer is at the start of memory block
        pHeader->checkValue = 0xDEED;
        pHeader->m_dataSize = size; //value of size in header equal to size of requested data 
        pHeader->m_totalDataSize = nRequestedBytes;
        pHeader->m_id = HeapID::Heap;
        pHeader->m_heap = nullptr; //set to nullptr for now.. after constructor, the heap header can point to itself
        pHeader->previous = nullptr;
        pHeader->next = nullptr;

        void* pFooterAddress = pMem + sizeof(Header) + size; //address of footer is past the header data and variable data
        Footer* pFooter = (Footer*)pFooterAddress;
        pFooter->m_id = HeapID::Heap;
        pFooter->checkValue = 0xFEED;

        void* pStartMemBlock = pMem + sizeof(Header);
        return pStartMemBlock;
    }
}

void operator delete(void* pMem, bool isHeap)
{
    if (!isHeap) {
        return ::operator delete(pMem);
    }
    else {
        Header* pHeader = (Header*)((char*)pMem - sizeof(Header));
        free(pHeader);
    }
}




//takes a pointer and returns the pointer to its header
Header* GetHeaderPntr(void* pntr)
{
    return (Header*)((char*)pntr - sizeof(Header));
}

//takes a pointer and returns a pointer to its footer
Footer* GetFooterPntr(void* pntr)
{
    Header* pHeader = GetHeaderPntr(pntr);
    return (Footer*)((char*)pntr + pHeader->m_dataSize);
}

//takes a header pointer and returns a pointer to the data
void* GetAddressFromHeader(Header* header)
{
    return (void*)((char*)header + sizeof(Header));
}

#endif // DEBUG