#include "vectorstorage.hh"
#include "FSBAllocator.hh"

#include <iostream>
#include <cstdlib>
#include <map>

VectorStorage::VecType VectorStorage::Buf;
VectorStorage::IDtype VectorStorage::BufOwner = 0;
VectorStorage::IDtype VectorStorage::ID = 0;
size_t VectorStorage::used = 0;

VectorStorage::DataStorageType VectorStorage::storage;
VectorStorage::PointerStorageType VectorStorage::ptrs;

void VectorStorage::StupidVec::Reassign(const VecType& vec)
{
    if(unlikely(vec.size() > ((LengthType)-1)))
    {
        std::cerr << "Apparently, byte was not enough for StupidVec::Length!\n";
        abort();
    }


    if(vec.size() > AllocLength)
    {
        unsigned DesiredSize = (vec.size() + 3) & ~3;
#if 1
        static std::multimap<LengthType/*size*/,unsigned/*pos*/,
            std::less<LengthType>,FSBAllocator<int> > holes;

        /*static unsigned counter=0;
        if(!counter--){counter=500000;
         std::cerr << "holes.size()="<<holes.size() << "\n";
        }*/

        if(storage.empty()) holes.clear();
        if(AllocLength > 0)
        {
            holes.insert(std::pair<LengthType,unsigned> (AllocLength+0, Ptr+0));
        }
        std::multimap<LengthType,unsigned,
            std::less<LengthType>,FSBAllocator<int>
                >::iterator h = holes.find(DesiredSize);
        if(h != holes.end())
        {
            Ptr = h->second;
            holes.erase(h);
        }
        else
#endif
        {
            Ptr = storage.size();
        }
        AllocLength = DesiredSize;
    }
    Length = vec.size();

    if(likely(Length > 0))
    {
        unsigned char* target = storage.GetWritePtr(Ptr, AllocLength);
        std::memcpy(target, vec.GetReadPtr(0,Length), Length);
    }
}

void VectorStorage::VecType::replace
    (size_t beginpos, size_t orig_size,
     const unsigned char* source, size_t new_size)
{
    if(orig_size == Length)
    {
        /* The whole content is being replaced */
        assign(source, source+new_size);
        return;
    }
    if(!owned)
    {
        assign(Buf, Buf+Length);
    }

    size_t tail_size = Length-(beginpos+orig_size);
    if(new_size > orig_size)
    {
        size_t diff = new_size - orig_size;
        Buf = (unsigned char*)std::realloc(Buf, Length + diff);
        std::memmove(Buf+beginpos+new_size,
                     Buf+beginpos+orig_size,
                     tail_size);
        Length += diff;
    }
    else if(new_size < orig_size)
    {
        size_t diff = orig_size - new_size;
        std::memmove(Buf+beginpos+new_size,
                     Buf+beginpos+orig_size,
                     tail_size);
        Buf = (unsigned char*)std::realloc(Buf, Length - diff);
        Length -= diff;
    }
    std::memcpy(Buf+beginpos, source, new_size);
    changed=true;
}

VectorStorage::IDtype VectorStorage::Create()
{
    ++used;

    StupidVec* ptr = ptrs.GetWritePtr(ID);
    // Construct the item in the buffer
    new(ptr) StupidVec;
    return ID++;
}

void VectorStorage::clear()
{
    ID=0;
    ptrs.clear();
    storage.clear();
    BufOwner=0; Buf.clear();
    used=0;
}

VectorStorage::VecType& VectorStorage::Read(IDtype n)
{
    if(n != BufOwner)
    {
        if(Buf.was_changed())
        {
            ptrs.GetWritePtr(BufOwner)->Reassign(Buf);
        }
        const StupidVec& v = *ptrs.GetReadPtr(BufOwner=n);

        const unsigned char* Source = v.GetBuf();
        Buf.initial_assign_readonly(Source, Source+v.GetLength());
    }
    return Buf;
}

void VectorStorage::Free(IDtype id)
{
    --used;
    if(!used) clear();
}
