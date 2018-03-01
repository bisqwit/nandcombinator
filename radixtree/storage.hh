#include <cstdlib>

/* MemoryStorage<datatype>
 * DiskStorage<datatype>
 *
 * Both implement an API for storing POD types
 * (i.e. data that does not mind getting moved around in memory).
 * They are automatically resized upon demand, and they
 * are accessed through GetWritePtr() and GetREadPtr() only.
 *
 * MemoryStorage uses virtual memory exclusively.
 * DiskStorage uses disk files exclusively,
 * with mmap() to provide access to them.
 */

template<typename DataType>
class MemoryStorage
{
private:
    unsigned char* buf;
    size_t Cap;
    size_t Size;
private:
    void Ensure(size_t endpos)
    {
        /* Ensure that the given amount of data exists */
        if(endpos > Size) Size = endpos;
        if(endpos > Cap)
        {
            size_t align = 0x100000;
            Cap = (endpos + align-1) & ~(align-1);
            
            /* Note: realloc(NULL,..) is equivalent to malloc(...) */
            buf = (unsigned char*)std::realloc(buf, Cap);
        }
    }
public:
    MemoryStorage(): buf(0), Cap(0), Size(0)
    {
    }
    ~MemoryStorage() { if(buf) std::free(buf); }
    
    inline bool empty() const   { return !Size; }
    inline size_t size() const { return Size; }
    inline void clear()
    {
        Size=0;
        //Cap=0;
        //if(buf) { std::free(buf); buf=0; }
    }

    /* Pointers returned by GetWritePtr() and GetReadPtr()
     * are guaranteed to be valid only until the next call
     * of either function or clear().
     */
    DataType* GetWritePtr(size_t pos, size_t wsize=1)
    {
        pos*=sizeof(DataType); wsize*=sizeof(DataType);
        Ensure(pos+wsize);
        return (DataType*)(buf+pos);
    }
    const DataType* GetReadPtr(size_t pos, size_t wsize=1)
    {
        pos*=sizeof(DataType); wsize*=sizeof(DataType);
        Ensure(pos+wsize);
        return (const DataType*)(buf+pos);
    }
private:
    /* Copying is not allowed.
     * However, this is not a singleton class.
     * Multiple instances can exist. Each has their own address space.
     */
    MemoryStorage(const MemoryStorage&);
    void operator=(const MemoryStorage&);
};
