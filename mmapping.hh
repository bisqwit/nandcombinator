#ifndef bqtMmappingHH
#define bqtMmappingHH

#include <unistd.h>
#include <sys/mman.h>

namespace {
static void* notmapped = (void*)-1;
}

enum { MMAP_PAGESIZE = 4096 };

template<bool AutoUnmap = true>
class MemMappingType
{
private:
public:
    MemMappingType() : ptr(notmapped), size(0), align_factor(0) { }

    MemMappingType(int fd, uint_fast64_t pos, uint_fast64_t length)
        : ptr(notmapped),size(0),align_factor(0)
    {
        SetMap(fd, pos, length);
    }

    ~MemMappingType()
    {
        if(AutoUnmap) Unmap();
    }

    void SetMap(int fd, uint_fast64_t pos, uint_fast64_t length, bool Write=false)
    {
        Unmap();

        uint_fast64_t pos_aligned_down = pos - (pos % MMAP_PAGESIZE);

        align_factor = pos - pos_aligned_down;

        size = length + align_factor;
        ptr =  mmap64(NULL, size,
                      Write ? (PROT_READ | PROT_WRITE)
                            : PROT_READ
                      , MAP_SHARED,
                      fd, pos_aligned_down);
    }

    bool ReMapIfNecessary(int /*fd*/, uint_fast64_t pos, uint_fast64_t length)
    {
        uint_fast64_t pos_aligned_down = pos - (pos % MMAP_PAGESIZE);
        size_t new_align_factor = pos - pos_aligned_down;
        size_t new_size         = length + align_factor;

        if(new_align_factor != align_factor
        || new_size         != size)
        {
            void *new_ptr = mremap(ptr, size, new_size, MREMAP_MAYMOVE);
            if(new_ptr == notmapped)
            {
                return false;
            }

            //fprintf(stderr, "did remap %lu->%lu\n", size, new_size);

            align_factor = new_align_factor;
            size         = new_size;
            ptr          = new_ptr;
        }
        /*else
            fprintf(stderr, "%lu=%lu, not remapping\n",
                size,new_size);*/
        return true;
    }

    void Unmap()
    {
        if(ptr != notmapped)
        {
            munmap(ptr, size);
            ptr = notmapped;
        }
    }

    void Sync()
    {
        if(ptr != notmapped)
        {
            msync(ptr, size, MS_SYNC);
        }
    }

    operator bool() const
        { return ptr != notmapped; }

    const unsigned char* get_ptr() const
        { return ((const unsigned char*)ptr) + align_factor; }

    unsigned char* get_write_ptr()
        { return ((unsigned char*)ptr) + align_factor; }

    /*
    operator const unsigned char*() const
        { return get_ptr(); }
    const unsigned char* operator+ (uint_fast64_t f) const
        { return get_ptr() + f; }
    */

    MemMappingType(const MemMappingType<AutoUnmap>& b)
        : ptr(b.ptr), size(b.size), align_factor(b.align_factor)
    {
    }
    MemMappingType& operator=(const MemMappingType<AutoUnmap>& b)
    {
        ptr = b.ptr;
        size = b.size;
        align_factor = b.align_factor;
        return *this;
    }

    MemMappingType(MemMappingType&& b)
        : ptr(b.ptr), size(b.size), align_factor(b.align_factor)
    {
        b.ptr = notmapped;
    }

    MemMappingType& operator=(MemMappingType&& b)
    {
        ptr  = b.ptr;
        size = b.size;
        align_factor = b.align_factor;
        if(&b != *this) b.ptr = notmapped;
        return *this;
    }

private:
    void* ptr;
    size_t size;
    size_t align_factor;
};

typedef MemMappingType<true> MemMapping;

#endif
